/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"

#include <memory>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_security_policy_violation_event_init.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/dom_string_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"
#include "third_party/blink/renderer/core/frame/csp/csp_source.h"
#include "third_party/blink/renderer/core/frame/csp/media_list_directive.h"
#include "third_party/blink/renderer/core/frame/csp/source_list_directive.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_response_headers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "v8/include/v8.h"

namespace blink {

using network::mojom::ContentSecurityPolicySource;
using network::mojom::ContentSecurityPolicyType;

namespace {

// Helper function that returns true if the given |header_type| should be
// checked when the CheckHeaderType is |check_header_type|.
bool CheckHeaderTypeMatches(
    ContentSecurityPolicy::CheckHeaderType check_header_type,
    ContentSecurityPolicyType header_type) {
  switch (check_header_type) {
    case ContentSecurityPolicy::CheckHeaderType::kCheckAll:
      return true;
    case ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly:
      return header_type == ContentSecurityPolicyType::kReport;
    case ContentSecurityPolicy::CheckHeaderType::kCheckEnforce:
      return header_type == ContentSecurityPolicyType::kEnforce;
  }
  NOTREACHED();
  return false;
}

}  // namespace

bool ContentSecurityPolicy::IsNonceableElement(const Element* element) {
  if (element->nonce().IsNull())
    return false;

  bool nonceable = true;

  // To prevent an attacker from hijacking an existing nonce via a dangling
  // markup injection, we walk through the attributes of each nonced script
  // element: if their names or values contain "<script" or "<style", we won't
  // apply the nonce when loading script.
  //
  // See http://blog.innerht.ml/csp-2015/#danglingmarkupinjection for an example
  // of the kind of attack this is aimed at mitigating.

  if (element->HasDuplicateAttribute())
    nonceable = false;

  if (nonceable) {
    static const char kScriptString[] = "<SCRIPT";
    static const char kStyleString[] = "<STYLE";
    for (const Attribute& attr : element->Attributes()) {
      const AtomicString& name = attr.LocalName();
      const AtomicString& value = attr.Value();
      if (name.FindIgnoringASCIICase(kScriptString) != WTF::kNotFound ||
          name.FindIgnoringASCIICase(kStyleString) != WTF::kNotFound ||
          value.FindIgnoringASCIICase(kScriptString) != WTF::kNotFound ||
          value.FindIgnoringASCIICase(kStyleString) != WTF::kNotFound) {
        nonceable = false;
        break;
      }
    }
  }

  UseCounter::Count(
      element->GetExecutionContext(),
      nonceable ? WebFeature::kCleanScriptElementWithNonce
                : WebFeature::kPotentiallyInjectedScriptElementWithNonce);

  return nonceable;
}

static WebFeature GetUseCounterHelperType(ContentSecurityPolicyType type) {
  switch (type) {
    case ContentSecurityPolicyType::kEnforce:
      return WebFeature::kContentSecurityPolicy;
    case ContentSecurityPolicyType::kReport:
      return WebFeature::kContentSecurityPolicyReportOnly;
  }
  NOTREACHED();
  return WebFeature::kNumberOfFeatures;
}

ContentSecurityPolicy::ContentSecurityPolicy()
    : delegate_(nullptr),
      override_inline_style_allowed_(false),
      script_hash_algorithms_used_(kContentSecurityPolicyHashAlgorithmNone),
      style_hash_algorithms_used_(kContentSecurityPolicyHashAlgorithmNone),
      sandbox_mask_(network::mojom::blink::WebSandboxFlags::kNone),
      require_trusted_types_(false),
      insecure_request_policy_(
          mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone) {}

bool ContentSecurityPolicy::IsBound() {
  return delegate_;
}

void ContentSecurityPolicy::BindToDelegate(
    ContentSecurityPolicyDelegate& delegate) {
  // TODO(crbug.com/915954): Add DCHECK(!delegate_). It seems some call sites
  // call this function multiple times.
  delegate_ = &delegate;
  ApplyPolicySideEffectsToDelegate();
}

void ContentSecurityPolicy::SetupSelf(const SecurityOrigin& security_origin) {
  // Ensure that 'self' processes correctly.
  self_protocol_ = security_origin.Protocol();
  self_source_ = MakeGarbageCollected<CSPSource>(
      this, self_protocol_, security_origin.Host(),
      // CSPSource uses port CSPSource::kPortUnspecified to represent a
      // missing port and reserves port 0 specifically for origins with port set
      // to 0; SecurityOrigin uses port 0 for origins with port 0 as well as for
      // origins without ports.
      //
      // TODO(crbug.com/1136678): Once SecurityOrigin starts treating port 0 as
      // a specifically set port, rather than as a sentinel for an
      // omitted or default-valued port, modify this logic.
      security_origin.Port() == 0 ? CSPSource::kPortUnspecified
                                  : security_origin.Port(),
      String(), CSPSource::kNoWildcard, CSPSource::kNoWildcard);
}

void ContentSecurityPolicy::SetupSelf(const ContentSecurityPolicy& other) {
  self_protocol_ = other.self_protocol_;
  if (other.self_source_) {
    self_source_ =
        MakeGarbageCollected<CSPSource>(this, *(other.self_source_.Get()));
  }
}

void ContentSecurityPolicy::ApplyPolicySideEffectsToDelegate() {
  DCHECK(delegate_);

  const SecurityOrigin* self_origin =
      delegate_->GetSecurityOrigin()->GetOriginOrPrecursorOriginIfOpaque();
  DCHECK(self_origin);

  SetupSelf(*self_origin);

  // Set mixed content checking and sandbox flags, then dump all the parsing
  // error messages, then poke at histograms.
  if (sandbox_mask_ != network::mojom::blink::WebSandboxFlags::kNone) {
    Count(WebFeature::kSandboxViaCSP);
    delegate_->SetSandboxFlags(sandbox_mask_);
  }

  if (require_trusted_types_) {
    delegate_->SetRequireTrustedTypes();
    Count(WebFeature::kTrustedTypesEnabled);
  }

  delegate_->AddInsecureRequestPolicy(insecure_request_policy_);

  for (const auto& console_message : console_messages_)
    delegate_->AddConsoleMessage(console_message);
  console_messages_.clear();

  for (const auto& policy : policies_) {
    Count(GetUseCounterHelperType(policy->HeaderType()));
    if (policy->AllowDynamic(
            ContentSecurityPolicy::DirectiveType::kScriptSrcAttr) ||
        policy->AllowDynamic(
            ContentSecurityPolicy::DirectiveType::kScriptSrcElem)) {
      Count(WebFeature::kCSPWithStrictDynamic);
    }

    if (policy->AllowEval(ReportingDisposition::kSuppressReporting,
                          kWillNotThrowException, g_empty_string)) {
      Count(WebFeature::kCSPWithUnsafeEval);
    }

    // We consider a policy to be "reasonably secure" if it:
    //
    // 1.  Asserts `object-src 'none'`.
    // 2.  Asserts `base-uri 'none'` or `base-uri 'self'`.
    // 3.  Avoids URL-based matching, in favor of hashes and nonces.
    //
    // https://chromium.googlesource.com/chromium/src/+/master/docs/security/web-mitigation-metrics.md
    // has more detail.
    if (policy->IsObjectRestrictionReasonable()) {
      Count(policy->HeaderType() == ContentSecurityPolicyType::kEnforce
                ? WebFeature::kCSPWithReasonableObjectRestrictions
                : WebFeature::kCSPROWithReasonableObjectRestrictions);
    }
    if (policy->IsBaseRestrictionReasonable()) {
      Count(policy->HeaderType() == ContentSecurityPolicyType::kEnforce
                ? WebFeature::kCSPWithReasonableBaseRestrictions
                : WebFeature::kCSPROWithReasonableBaseRestrictions);
    }
    if (policy->IsScriptRestrictionReasonable()) {
      Count(policy->HeaderType() == ContentSecurityPolicyType::kEnforce
                ? WebFeature::kCSPWithReasonableScriptRestrictions
                : WebFeature::kCSPROWithReasonableScriptRestrictions);
    }
    if (policy->IsObjectRestrictionReasonable() &&
        policy->IsBaseRestrictionReasonable() &&
        policy->IsScriptRestrictionReasonable()) {
      Count(policy->HeaderType() == ContentSecurityPolicyType::kEnforce
                ? WebFeature::kCSPWithReasonableRestrictions
                : WebFeature::kCSPROWithReasonableRestrictions);

      if (!policy->AllowDynamic(
              ContentSecurityPolicy::DirectiveType::kScriptSrcElem)) {
        Count(policy->HeaderType() == ContentSecurityPolicyType::kEnforce
                  ? WebFeature::kCSPWithBetterThanReasonableRestrictions
                  : WebFeature::kCSPROWithBetterThanReasonableRestrictions);
      }
    }
    if (policy->RequiresTrustedTypes()) {
      Count(policy->IsReportOnly() ? WebFeature::kTrustedTypesEnabledReportOnly
                                   : WebFeature::kTrustedTypesEnabledEnforcing);
    }
    if (policy->TrustedTypesAllowDuplicates()) {
      Count(WebFeature::kTrustedTypesAllowDuplicates);
    }
  }

  // We disable 'eval()' even in the case of report-only policies, and rely on
  // the check in the V8Initializer::codeGenerationCheckCallbackInMainThread
  // callback to determine whether the call should execute or not.
  if (!disable_eval_error_message_.IsNull())
    delegate_->DisableEval(disable_eval_error_message_);
}

ContentSecurityPolicy::~ContentSecurityPolicy() = default;

void ContentSecurityPolicy::Trace(Visitor* visitor) const {
  visitor->Trace(delegate_);
  visitor->Trace(policies_);
  visitor->Trace(console_messages_);
  visitor->Trace(self_source_);
}

void ContentSecurityPolicy::CopyStateFrom(const ContentSecurityPolicy* other) {
  DCHECK(policies_.IsEmpty());
  for (const auto& policy : other->policies_)
    AddAndReportPolicyFromHeaderValue(policy->Header(), policy->HeaderType(),
                                      policy->HeaderSource());
  SetupSelf(*other);
}

void ContentSecurityPolicy::CopyPluginTypesFrom(
    const ContentSecurityPolicy* other) {
  for (const auto& policy : other->policies_) {
    if (policy->HasPluginTypes()) {
      AddAndReportPolicyFromHeaderValue(policy->PluginTypesText(),
                                        policy->HeaderType(),
                                        policy->HeaderSource());
    }
  }
}

void ContentSecurityPolicy::DidReceiveHeaders(
    const ContentSecurityPolicyResponseHeaders& headers) {
  if (headers.ShouldParseWasmEval())
    supports_wasm_eval_ = true;
  if (!headers.ContentSecurityPolicy().IsEmpty()) {
    AddAndReportPolicyFromHeaderValue(headers.ContentSecurityPolicy(),
                                      ContentSecurityPolicyType::kEnforce,
                                      ContentSecurityPolicySource::kHTTP);
  }
  if (!headers.ContentSecurityPolicyReportOnly().IsEmpty()) {
    AddAndReportPolicyFromHeaderValue(headers.ContentSecurityPolicyReportOnly(),
                                      ContentSecurityPolicyType::kReport,
                                      ContentSecurityPolicySource::kHTTP);
  }
}

void ContentSecurityPolicy::DidReceiveHeader(
    const String& header,
    ContentSecurityPolicyType type,
    ContentSecurityPolicySource source) {
  AddAndReportPolicyFromHeaderValue(header, type, source);

  // This might be called after we've been bound to a delegate. For example, a
  // <meta> element might be injected after page load.
  if (delegate_)
    ApplyPolicySideEffectsToDelegate();
}

void ContentSecurityPolicy::AddPolicyFromHeaderValue(
    const String& header,
    ContentSecurityPolicyType type,
    ContentSecurityPolicySource source) {
  // If this is a report-only header inside a <meta> element, bail out.
  if (source == ContentSecurityPolicySource::kMeta &&
      type == ContentSecurityPolicyType::kReport) {
    ReportReportOnlyInMeta(header);
    return;
  }

  if (source == ContentSecurityPolicySource::kHTTP)
    header_delivered_ = true;

  Vector<UChar> characters;
  header.AppendTo(characters);

  const UChar* begin = characters.data();
  const UChar* end = begin + characters.size();

  // RFC2616, section 4.2 specifies that headers appearing multiple times can
  // be combined with a comma. Walk the header string, and parse each comma
  // separated chunk as a separate header.
  const UChar* position = begin;
  while (position < end) {
    SkipUntil<UChar>(position, end, ',');

    // header1,header2 OR header1
    //        ^                  ^
    Member<CSPDirectiveList> policy =
        CSPDirectiveList::Create(this, begin, position, type, source);

    if (policy->ShouldDisableEval() && disable_eval_error_message_.IsNull()) {
      disable_eval_error_message_ = policy->EvalDisabledErrorMessage();
    }

    policies_.push_back(policy.Release());

    // Skip the comma, and begin the next header from the current position.
    DCHECK(position == end || *position == ',');
    SkipExactly<UChar>(position, end, ',');
    begin = position;
  }
}

void ContentSecurityPolicy::ReportAccumulatedHeaders() const {
  WTF::Vector<network::mojom::blink::ContentSecurityPolicyPtr> policies;
  for (const auto& policy : policies_)
    policies.push_back(policy->ExposeForNavigationalChecks());

  DCHECK(delegate_);
  delegate_->DidAddContentSecurityPolicies(std::move(policies));
}

void ContentSecurityPolicy::AddAndReportPolicyFromHeaderValue(
    const String& header,
    ContentSecurityPolicyType type,
    ContentSecurityPolicySource source) {
  wtf_size_t previous_policy_count = policies_.size();
  AddPolicyFromHeaderValue(header, type, source);
  // Notify about the new header, so that it can be reported back to the
  // browser process.  This is needed in order to:
  // 1) replicate CSP directives (i.e. frame-src) to OOPIFs (only for now /
  // short-term).
  // 2) enforce CSP in the browser process (long-term - see
  // https://crbug.com/376522).
  // TODO(arthursonzogni): policies are actually replicated (1) and some of
  // them are enforced on the browser process (2). Stop doing (1) when (2) is
  // finished.
  WTF::Vector<network::mojom::blink::ContentSecurityPolicyPtr> policies(
      policies_.size() - previous_policy_count);
  for (wtf_size_t i = previous_policy_count; i < policies_.size(); ++i) {
    policies[i - previous_policy_count] =
        policies_[i]->ExposeForNavigationalChecks();
  }

  if (delegate_)
    delegate_->DidAddContentSecurityPolicies(std::move(policies));
}

void ContentSecurityPolicy::SetOverrideAllowInlineStyle(bool value) {
  override_inline_style_allowed_ = value;
}

void ContentSecurityPolicy::SetOverrideURLForSelf(const KURL& url) {
  // Create a temporary CSPSource so that 'self' expressions can be resolved
  // before we bind to an execution context (for 'frame-ancestor' resolution,
  // for example). This CSPSource will be overwritten when we bind this object
  // to an execution context.
  scoped_refptr<const SecurityOrigin> origin = SecurityOrigin::Create(url);
  self_protocol_ = origin->Protocol();
  self_source_ = MakeGarbageCollected<CSPSource>(
      this, self_protocol_, origin->Host(), origin->Port(), String(),
      CSPSource::kNoWildcard, CSPSource::kNoWildcard);
}

Vector<CSPHeaderAndType> ContentSecurityPolicy::Headers() const {
  Vector<CSPHeaderAndType> headers;
  headers.ReserveInitialCapacity(policies_.size());
  for (const auto& policy : policies_) {
    headers.UncheckedAppend(
        CSPHeaderAndType(policy->Header(), policy->HeaderType()));
  }
  return headers;
}

// static
void ContentSecurityPolicy::FillInCSPHashValues(
    const String& source,
    uint8_t hash_algorithms_used,
    Vector<CSPHashValue>* csp_hash_values) {
  // Any additions or subtractions from this struct should also modify the
  // respective entries in the kSupportedPrefixes array in
  // SourceListDirective::parseHash().
  static const struct {
    ContentSecurityPolicyHashAlgorithm csp_hash_algorithm;
    HashAlgorithm algorithm;
  } kAlgorithmMap[] = {
      {kContentSecurityPolicyHashAlgorithmSha256, kHashAlgorithmSha256},
      {kContentSecurityPolicyHashAlgorithmSha384, kHashAlgorithmSha384},
      {kContentSecurityPolicyHashAlgorithmSha512, kHashAlgorithmSha512}};

  // Only bother normalizing the source/computing digests if there are any
  // checks to be done.
  if (hash_algorithms_used == kContentSecurityPolicyHashAlgorithmNone)
    return;

  StringUTF8Adaptor utf8_source(
      source, kStrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD);

  for (const auto& algorithm_map : kAlgorithmMap) {
    DigestValue digest;
    if (algorithm_map.csp_hash_algorithm & hash_algorithms_used) {
      bool digest_success =
          ComputeDigest(algorithm_map.algorithm, utf8_source.data(),
                        utf8_source.size(), digest);
      if (digest_success) {
        csp_hash_values->push_back(
            CSPHashValue(algorithm_map.csp_hash_algorithm, digest));
      }
    }
  }
}

// static
bool ContentSecurityPolicy::CheckHashAgainstPolicy(
    Vector<CSPHashValue>& csp_hash_values,
    const Member<CSPDirectiveList>& policy,
    InlineType inline_type) {
  for (const auto& csp_hash_value : csp_hash_values) {
    if (policy->AllowHash(csp_hash_value, inline_type))
      return true;
  }
  return false;
}

// https://w3c.github.io/webappsec-csp/#should-block-inline
bool ContentSecurityPolicy::AllowInline(
    InlineType inline_type,
    Element* element,
    const String& content,
    const String& nonce,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    ReportingDisposition reporting_disposition) const {
  DCHECK(element || inline_type == InlineType::kScriptAttribute ||
         inline_type == InlineType::kNavigation);

  const bool is_script = IsScriptInlineType(inline_type);
  if (!is_script && override_inline_style_allowed_) {
    return true;
  }

  Vector<CSPHashValue> csp_hash_values;
  FillInCSPHashValues(
      content,
      is_script ? script_hash_algorithms_used_ : style_hash_algorithms_used_,
      &csp_hash_values);

  // Step 2. Let result be "Allowed". [spec text]
  bool is_allowed = true;

  // Step 3. For each policy in element’s Document's global object’s CSP list:
  // [spec text]
  for (const auto& policy : policies_) {
    // May be allowed by hash, if 'unsafe-hashes' is present in a policy.
    // Check against the digest of the |content| and also check whether inline
    // script is allowed.
    is_allowed &=
        CheckHashAgainstPolicy(csp_hash_values, policy, inline_type) ||
        policy->AllowInline(inline_type, element, content, nonce, context_url,
                            context_line, reporting_disposition);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::IsScriptInlineType(InlineType inline_type) {
  switch (inline_type) {
    case ContentSecurityPolicy::InlineType::kNavigation:
    case ContentSecurityPolicy::InlineType::kScriptAttribute:
    case ContentSecurityPolicy::InlineType::kScript:
      return true;

    case ContentSecurityPolicy::InlineType::kStyleAttribute:
    case ContentSecurityPolicy::InlineType::kStyle:
      return false;
  }
}

bool ContentSecurityPolicy::ShouldCheckEval() const {
  for (const auto& policy : policies_) {
    if (policy->ShouldCheckEval())
      return true;
  }
  return IsRequireTrustedTypes();
}

bool ContentSecurityPolicy::AllowEval(
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& script_content) const {
  bool is_allowed = true;
  for (const auto& policy : policies_) {
    is_allowed &= policy->AllowEval(reporting_disposition, exception_status,
                                    script_content);
  }
  return is_allowed;
}

bool ContentSecurityPolicy::AllowWasmEval(
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& script_content) const {
  bool is_allowed = true;
  for (const auto& policy : policies_) {
    is_allowed &= policy->AllowWasmEval(reporting_disposition, exception_status,
                                        script_content);
  }
  return is_allowed;
}

String ContentSecurityPolicy::EvalDisabledErrorMessage() const {
  for (const auto& policy : policies_) {
    if (policy->ShouldDisableEval())
      return policy->EvalDisabledErrorMessage();
  }
  return String();
}

bool ContentSecurityPolicy::AllowPluginType(
    const String& type,
    const String& type_attribute,
    const KURL& url,
    ReportingDisposition reporting_disposition) const {
  for (const auto& policy : policies_) {
    if (!policy->AllowPluginType(type, type_attribute, url,
                                 reporting_disposition))
      return false;
  }
  return true;
}

static base::Optional<ContentSecurityPolicy::DirectiveType>
GetDirectiveTypeFromRequestContextType(
    mojom::blink::RequestContextType context) {
  switch (context) {
    case mojom::blink::RequestContextType::AUDIO:
    case mojom::blink::RequestContextType::TRACK:
    case mojom::blink::RequestContextType::VIDEO:
      return ContentSecurityPolicy::DirectiveType::kMediaSrc;

    case mojom::blink::RequestContextType::BEACON:
    case mojom::blink::RequestContextType::EVENT_SOURCE:
    case mojom::blink::RequestContextType::FETCH:
    case mojom::blink::RequestContextType::PING:
    case mojom::blink::RequestContextType::XML_HTTP_REQUEST:
    case mojom::blink::RequestContextType::SUBRESOURCE:
      return ContentSecurityPolicy::DirectiveType::kConnectSrc;

    case mojom::blink::RequestContextType::EMBED:
    case mojom::blink::RequestContextType::OBJECT:
      return ContentSecurityPolicy::DirectiveType::kObjectSrc;

    case mojom::blink::RequestContextType::PREFETCH:
      return ContentSecurityPolicy::DirectiveType::kPrefetchSrc;

    case mojom::blink::RequestContextType::FAVICON:
    case mojom::blink::RequestContextType::IMAGE:
    case mojom::blink::RequestContextType::IMAGE_SET:
      return ContentSecurityPolicy::DirectiveType::kImgSrc;

    case mojom::blink::RequestContextType::FONT:
      return ContentSecurityPolicy::DirectiveType::kFontSrc;

    case mojom::blink::RequestContextType::FORM:
      return ContentSecurityPolicy::DirectiveType::kFormAction;

    case mojom::blink::RequestContextType::FRAME:
    case mojom::blink::RequestContextType::IFRAME:
      return ContentSecurityPolicy::DirectiveType::kFrameSrc;

    case mojom::blink::RequestContextType::IMPORT:
    case mojom::blink::RequestContextType::SCRIPT:
    case mojom::blink::RequestContextType::XSLT:
      return ContentSecurityPolicy::DirectiveType::kScriptSrcElem;

    case mojom::blink::RequestContextType::MANIFEST:
      return ContentSecurityPolicy::DirectiveType::kManifestSrc;

    case mojom::blink::RequestContextType::SERVICE_WORKER:
    case mojom::blink::RequestContextType::SHARED_WORKER:
    case mojom::blink::RequestContextType::WORKER:
      return ContentSecurityPolicy::DirectiveType::kWorkerSrc;

    case mojom::blink::RequestContextType::STYLE:
      return ContentSecurityPolicy::DirectiveType::kStyleSrcElem;

    case mojom::blink::RequestContextType::CSP_REPORT:
    case mojom::blink::RequestContextType::DOWNLOAD:
    case mojom::blink::RequestContextType::HYPERLINK:
    case mojom::blink::RequestContextType::INTERNAL:
    case mojom::blink::RequestContextType::LOCATION:
    case mojom::blink::RequestContextType::PLUGIN:
    case mojom::blink::RequestContextType::UNSPECIFIED:
      return base::nullopt;
  }
}

bool ContentSecurityPolicy::AllowRequest(
    mojom::blink::RequestContextType context,
    network::mojom::RequestDestination request_destination,
    const KURL& url,
    const String& nonce,
    const IntegrityMetadataSet& integrity_metadata,
    ParserDisposition parser_disposition,
    const KURL& url_before_redirects,
    RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    CheckHeaderType check_header_type) const {
  base::Optional<ContentSecurityPolicy::DirectiveType> type =
      GetDirectiveTypeFromRequestContextType(context);

  if (!type)
    return true;
  return AllowFromSource(*type, url, url_before_redirects, redirect_status,
                         reporting_disposition, check_header_type, nonce,
                         integrity_metadata, parser_disposition);
}

void ContentSecurityPolicy::UsesScriptHashAlgorithms(uint8_t algorithms) {
  script_hash_algorithms_used_ |= algorithms;
}

void ContentSecurityPolicy::UsesStyleHashAlgorithms(uint8_t algorithms) {
  style_hash_algorithms_used_ |= algorithms;
}

bool ContentSecurityPolicy::AllowFromSource(
    ContentSecurityPolicy::DirectiveType type,
    const KURL& url,
    const KURL& url_before_redirects,
    RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    CheckHeaderType check_header_type,
    const String& nonce,
    const IntegrityMetadataSet& hashes,
    ParserDisposition parser_disposition) const {
  SchemeRegistry::PolicyAreas area = SchemeRegistry::kPolicyAreaAll;
  if (type == ContentSecurityPolicy::DirectiveType::kImgSrc)
    area = SchemeRegistry::kPolicyAreaImage;
  else if (type == ContentSecurityPolicy::DirectiveType::kStyleSrcElem)
    area = SchemeRegistry::kPolicyAreaStyle;

  if (ShouldBypassContentSecurityPolicy(url, area)) {
    if (type != ContentSecurityPolicy::DirectiveType::kScriptSrcElem)
      return true;

    Count(parser_disposition == kParserInserted
              ? WebFeature::kScriptWithCSPBypassingSchemeParserInserted
              : WebFeature::kScriptWithCSPBypassingSchemeNotParserInserted);

    // If we're running experimental features, bypass CSP only for
    // non-parser-inserted resources whose scheme otherwise bypasses CSP. If
    // we're not running experimental features, bypass CSP for all resources
    // regardless of parser state. Once we have more data via the
    // 'ScriptWithCSPBypassingScheme*' metrics, make a decision about what
    // behavior to ship. https://crbug.com/653521
    if ((parser_disposition == kNotParserInserted ||
         !ExperimentalFeaturesEnabled()) &&
        // The schemes where javascript:-URLs are blocked are usually
        // privileged pages, so do not allow the CSP to be bypassed either.
        !SchemeRegistry::ShouldTreatURLSchemeAsNotAllowingJavascriptURLs(
            delegate_->GetSecurityOrigin()->Protocol())) {
      return true;
    }
  }

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
      continue;
    is_allowed &= policy->AllowFromSource(
        type, url, url_before_redirects, redirect_status, reporting_disposition,
        nonce, hashes, parser_disposition);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowBaseURI(const KURL& url) const {
  // `base-uri` isn't affected by 'upgrade-insecure-requests', so we use
  // CheckHeaderType::kCheckAll to check both report-only and enforce headers
  // here.
  return AllowFromSource(ContentSecurityPolicy::DirectiveType::kBaseURI, url,
                         url, RedirectStatus::kNoRedirect);
}

bool ContentSecurityPolicy::AllowConnectToSource(
    const KURL& url,
    const KURL& url_before_redirects,
    RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    CheckHeaderType check_header_type) const {
  return AllowFromSource(ContentSecurityPolicy::DirectiveType::kConnectSrc, url,
                         url_before_redirects, redirect_status,
                         reporting_disposition, check_header_type);
}

bool ContentSecurityPolicy::AllowFormAction(const KURL& url) const {
  return AllowFromSource(ContentSecurityPolicy::DirectiveType::kFormAction, url,
                         url, RedirectStatus::kNoRedirect);
}

bool ContentSecurityPolicy::AllowImageFromSource(
    const KURL& url,
    const KURL& url_before_redirects,
    RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    CheckHeaderType check_header_type) const {
  return AllowFromSource(ContentSecurityPolicy::DirectiveType::kImgSrc, url,
                         url_before_redirects, redirect_status,
                         reporting_disposition, check_header_type);
}

bool ContentSecurityPolicy::AllowMediaFromSource(const KURL& url) const {
  return AllowFromSource(ContentSecurityPolicy::DirectiveType::kMediaSrc, url,
                         url, RedirectStatus::kNoRedirect);
}

bool ContentSecurityPolicy::AllowObjectFromSource(const KURL& url) const {
  return AllowFromSource(ContentSecurityPolicy::DirectiveType::kObjectSrc, url,
                         url, RedirectStatus::kNoRedirect);
}

bool ContentSecurityPolicy::AllowScriptFromSource(
    const KURL& url,
    const String& nonce,
    const IntegrityMetadataSet& hashes,
    ParserDisposition parser_disposition,
    const KURL& url_before_redirects,
    RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    CheckHeaderType check_header_type) const {
  return AllowFromSource(ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
                         url, url_before_redirects, redirect_status,
                         reporting_disposition, check_header_type, nonce,
                         hashes, parser_disposition);
}

bool ContentSecurityPolicy::AllowWorkerContextFromSource(
    const KURL& url) const {
  return AllowFromSource(ContentSecurityPolicy::DirectiveType::kWorkerSrc, url,
                         url, RedirectStatus::kNoRedirect);
}

// The return value indicates whether the policy is allowed or not.
// If the return value is false, the out-parameter violation_details indicates
// the type of the violation, and if the return value is true,
// it indicates if a report-only violation occurred.
bool ContentSecurityPolicy::AllowTrustedTypePolicy(
    const String& policy_name,
    bool is_duplicate,
    AllowTrustedTypePolicyDetails& violation_details) const {
  bool is_allowed = true;
  violation_details = AllowTrustedTypePolicyDetails::kAllowed;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(CheckHeaderType::kCheckAll,
                                policy->HeaderType())) {
      continue;
    }
    auto new_violation_details = AllowTrustedTypePolicyDetails::kAllowed;
    bool new_allowed = policy->AllowTrustedTypePolicy(policy_name, is_duplicate,
                                                      new_violation_details);
    // Report the first violation that is enforced.
    // If there is none, report the first violation that is report-only.
    if ((is_allowed && !new_allowed) ||
        violation_details == AllowTrustedTypePolicyDetails::kAllowed) {
      violation_details = new_violation_details;
    }
    is_allowed &= new_allowed;
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowTrustedTypeAssignmentFailure(
    const String& message,
    const String& sample,
    const String& sample_prefix) const {
  bool allow = true;
  for (const auto& policy : policies_) {
    allow &= policy->AllowTrustedTypeAssignmentFailure(message, sample,
                                                       sample_prefix);
  }
  return allow;
}

bool ContentSecurityPolicy::IsActive() const {
  return !policies_.IsEmpty();
}

bool ContentSecurityPolicy::IsActiveForConnections() const {
  for (const auto& policy : policies_) {
    if (policy->IsActiveForConnections())
      return true;
  }
  return false;
}

const KURL ContentSecurityPolicy::FallbackUrlForPlugin() const {
  return delegate_ ? delegate_->Url() : KURL();
}

void ContentSecurityPolicy::EnforceSandboxFlags(
    network::mojom::blink::WebSandboxFlags mask) {
  sandbox_mask_ |= mask;
}

void ContentSecurityPolicy::RequireTrustedTypes() {
  // We store whether CSP demands a policy. The caller still needs to check
  // whether the feature is enabled in the first place.
  require_trusted_types_ = true;
}

void ContentSecurityPolicy::EnforceStrictMixedContentChecking() {
  insecure_request_policy_ |=
      mojom::blink::InsecureRequestPolicy::kBlockAllMixedContent;
}

void ContentSecurityPolicy::UpgradeInsecureRequests() {
  insecure_request_policy_ |=
      mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests;
}

static String StripURLForUseInReport(
    const SecurityOrigin* security_origin,
    const KURL& url,
    RedirectStatus redirect_status,
    const ContentSecurityPolicy::DirectiveType& effective_type) {
  if (!url.IsValid())
    return String();
  if (!url.IsHierarchical() || url.ProtocolIs("file"))
    return url.Protocol();

  // Until we're more careful about the way we deal with navigations in frames
  // (and, by extension, in plugin documents), strip cross-origin 'frame-src'
  // and 'object-src' violations down to an origin. https://crbug.com/633306
  bool can_safely_expose_url =
      security_origin->CanRequest(url) ||
      (redirect_status == RedirectStatus::kNoRedirect &&
       effective_type != ContentSecurityPolicy::DirectiveType::kFrameSrc &&
       effective_type != ContentSecurityPolicy::DirectiveType::kObjectSrc);

  if (can_safely_expose_url) {
    // 'KURL::strippedForUseAsReferrer()' dumps 'String()' for non-webby URLs.
    // It's better for developers if we return the origin of those URLs rather
    // than nothing.
    if (url.ProtocolIsInHTTPFamily())
      return url.StrippedForUseAsReferrer();
  }
  return SecurityOrigin::Create(url)->ToString();
}

static void GatherSecurityPolicyViolationEventData(
    SecurityPolicyViolationEventInit* init,
    ContentSecurityPolicyDelegate* delegate,
    const String& directive_text,
    const ContentSecurityPolicy::DirectiveType& effective_type,
    const KURL& blocked_url,
    const String& header,
    RedirectStatus redirect_status,
    ContentSecurityPolicyType header_type,
    ContentSecurityPolicy::ContentSecurityPolicyViolationType violation_type,
    std::unique_ptr<SourceLocation> source_location,
    const String& script_source,
    const String& sample_prefix) {
  if (effective_type == ContentSecurityPolicy::DirectiveType::kFrameAncestors) {
    // If this load was blocked via 'frame-ancestors', then the URL of
    // |document| has not yet been initialized. In this case, we'll set both
    // 'documentURI' and 'blockedURI' to the blocked document's URL.
    String stripped_url = StripURLForUseInReport(
        delegate->GetSecurityOrigin(), blocked_url, RedirectStatus::kNoRedirect,
        ContentSecurityPolicy::DirectiveType::kDefaultSrc);
    init->setDocumentURI(stripped_url);
    init->setBlockedURI(stripped_url);
  } else {
    String stripped_url = StripURLForUseInReport(
        delegate->GetSecurityOrigin(), delegate->Url(),
        RedirectStatus::kNoRedirect,
        ContentSecurityPolicy::DirectiveType::kDefaultSrc);
    init->setDocumentURI(stripped_url);
    switch (violation_type) {
      case ContentSecurityPolicy::kInlineViolation:
        init->setBlockedURI("inline");
        break;
      case ContentSecurityPolicy::kEvalViolation:
        init->setBlockedURI("eval");
        break;
      case ContentSecurityPolicy::kURLViolation:
        // We pass RedirectStatus::kNoRedirect so that StripURLForUseInReport
        // does not strip path and query from the URL. This is safe since
        // blocked_url at this point is always the original url (before
        // redirects).
        init->setBlockedURI(StripURLForUseInReport(
            delegate->GetSecurityOrigin(), blocked_url,
            RedirectStatus::kNoRedirect, effective_type));
        break;
      case ContentSecurityPolicy::kTrustedTypesSinkViolation:
        init->setBlockedURI("trusted-types-sink");
        break;
      case ContentSecurityPolicy::kTrustedTypesPolicyViolation:
        init->setBlockedURI("trusted-types-policy");
        break;
    }
  }

  String effective_directive =
      ContentSecurityPolicy::GetDirectiveName(effective_type);
  init->setViolatedDirective(effective_directive);
  init->setEffectiveDirective(effective_directive);
  init->setOriginalPolicy(header);
  init->setDisposition(header_type == ContentSecurityPolicyType::kEnforce
                           ? "enforce"
                           : "report");
  init->setStatusCode(0);

  // See https://w3c.github.io/webappsec-csp/#create-violation-for-global.
  // Step 3. If global is a Window object, set violation’s referrer to global’s
  // document's referrer. [spec text]
  String referrer = delegate->GetDocumentReferrer();
  if (referrer)
    init->setReferrer(referrer);

  // Step 4. Set violation’s status to the HTTP status code for the resource
  // associated with violation’s global object. [spec text]
  base::Optional<uint16_t> status_code = delegate->GetStatusCode();
  if (status_code)
    init->setStatusCode(*status_code);

  // If no source location is provided, use the source location from the
  // |delegate|.
  // Step 2. If the user agent is currently executing script, and can extract a
  // source file’s URL, line number, and column number from the global, set
  // violation’s source file, line number, and column number accordingly.
  // [spec text]
  if (!source_location)
    source_location = delegate->GetSourceLocation();
  if (source_location && source_location->LineNumber()) {
    KURL source_url = KURL(source_location->Url());
    // The source file might be a script loaded from a redirect. Web browser
    // usually tries to hide post-redirect information. The script might be
    // cross-origin with the document, but also with other scripts. As a result,
    // everything is cleared no matter the |source_url| origin.
    // See https://crbug.com/1074317
    //
    // Note: The username, password and ref are stripped later below by
    // StripURLForUseInReport(..)
    source_url.SetQuery(String());

    // The |source_url| is the URL of the script that triggered the CSP
    // violation. It is the URL pre-redirect. So it is safe to expose it in
    // reports without leaking any new informations to the document. See
    // https://crrev.com/c/2187792.
    String source_file =
        StripURLForUseInReport(delegate->GetSecurityOrigin(), source_url,
                               RedirectStatus::kNoRedirect, effective_type);

    init->setSourceFile(source_file);
    init->setLineNumber(source_location->LineNumber());
    init->setColumnNumber(source_location->ColumnNumber());
  } else {
    init->setSourceFile(String());
    init->setLineNumber(0);
    init->setColumnNumber(0);
  }

  // Build the sample string. CSP demands that the sample is restricted to
  // 40 characters (kMaxSampleLength), to prevent inadvertent exfiltration of
  // user data. For some use cases, we also have a sample prefix, which
  // must not depend on user data and where we will apply the sample limit
  // separately.
  StringBuilder sample;
  if (!sample_prefix.IsEmpty()) {
    sample.Append(sample_prefix.StripWhiteSpace().Left(
        ContentSecurityPolicy::kMaxSampleLength));
    sample.Append("|");
  }
  if (!script_source.IsEmpty()) {
    sample.Append(script_source.StripWhiteSpace().Left(
        ContentSecurityPolicy::kMaxSampleLength));
  }
  if (!sample.IsEmpty())
    init->setSample(sample.ToString());
}

void ContentSecurityPolicy::ReportViolation(
    const String& directive_text,
    const DirectiveType& effective_type,
    const String& console_message,
    const KURL& blocked_url,
    const Vector<String>& report_endpoints,
    bool use_reporting_api,
    const String& header,
    ContentSecurityPolicyType header_type,
    ContentSecurityPolicyViolationType violation_type,
    std::unique_ptr<SourceLocation> source_location,
    LocalFrame* context_frame,
    RedirectStatus redirect_status,
    Element* element,
    const String& source,
    const String& source_prefix) {
  DCHECK(violation_type == kURLViolation || blocked_url.IsEmpty());

  // TODO(lukasza): Support sending reports from OOPIFs -
  // https://crbug.com/611232 (or move CSP child-src and frame-src checks to the
  // browser process - see https://crbug.com/376522).
  if (!delegate_ && !context_frame) {
    DCHECK(effective_type == DirectiveType::kChildSrc ||
           effective_type == DirectiveType::kFrameSrc ||
           effective_type == DirectiveType::kPluginTypes ||
           effective_type == DirectiveType::kTrustedTypes ||
           effective_type == DirectiveType::kRequireTrustedTypesFor);
    return;
  }
  DCHECK((delegate_ && !context_frame) ||
         ((effective_type == DirectiveType::kFrameAncestors) && context_frame));

  SecurityPolicyViolationEventInit* violation_data =
      SecurityPolicyViolationEventInit::Create();

  // If we're processing 'frame-ancestors', use the delegate for the
  // |context_frame|'s document to gather data. Otherwise, use the policy's
  // |delegate_|.
  ContentSecurityPolicyDelegate* relevant_delegate =
      context_frame
          ? &context_frame->DomWindow()->GetContentSecurityPolicyDelegate()
          : delegate_.Get();
  DCHECK(relevant_delegate);
  GatherSecurityPolicyViolationEventData(
      violation_data, relevant_delegate, directive_text, effective_type,
      blocked_url, header, redirect_status, header_type, violation_type,
      std::move(source_location), source, source_prefix);

  // TODO(mkwst): Obviously, we shouldn't hit this check, as extension-loaded
  // resources should be allowed regardless. We apparently do, however, so
  // we should at least stop spamming reporting endpoints. See
  // https://crbug.com/524356 for detail.
  if (!violation_data->sourceFile().IsEmpty() &&
      ShouldBypassContentSecurityPolicy(KURL(violation_data->sourceFile()))) {
    return;
  }

  PostViolationReport(violation_data, context_frame, report_endpoints,
                      use_reporting_api);

  // Fire a violation event if we're working with a delegate (e.g. we're not
  // processing 'frame-ancestors').
  if (delegate_)
    delegate_->DispatchViolationEvent(*violation_data, element);

  ReportContentSecurityPolicyIssue(*violation_data, header_type, violation_type,
                                   context_frame, element);
}

void ContentSecurityPolicy::PostViolationReport(
    const SecurityPolicyViolationEventInit* violation_data,
    LocalFrame* context_frame,
    const Vector<String>& report_endpoints,
    bool use_reporting_api) {
  // We need to be careful here when deciding what information to send to the
  // report-uri. Currently, we send only the current document's URL and the
  // directive that was violated. The document's URL is safe to send because
  // it's the document itself that's requesting that it be sent. You could
  // make an argument that we shouldn't send HTTPS document URLs to HTTP
  // report-uris (for the same reasons that we supress the Referer in that
  // case), but the Referer is sent implicitly whereas this request is only
  // sent explicitly. As for which directive was violated, that's pretty
  // harmless information.
  //
  // TODO(mkwst): This justification is BS. Insecure reports are mixed content,
  // let's kill them. https://crbug.com/695363

  auto csp_report = std::make_unique<JSONObject>();
  csp_report->SetString("document-uri", violation_data->documentURI());
  csp_report->SetString("referrer", violation_data->referrer());
  csp_report->SetString("violated-directive",
                        violation_data->violatedDirective());
  csp_report->SetString("effective-directive",
                        violation_data->effectiveDirective());
  csp_report->SetString("original-policy", violation_data->originalPolicy());
  csp_report->SetString("disposition", violation_data->disposition());
  csp_report->SetString("blocked-uri", violation_data->blockedURI());
  if (violation_data->lineNumber())
    csp_report->SetInteger("line-number", violation_data->lineNumber());
  if (violation_data->columnNumber())
    csp_report->SetInteger("column-number", violation_data->columnNumber());
  if (!violation_data->sourceFile().IsEmpty())
    csp_report->SetString("source-file", violation_data->sourceFile());
  csp_report->SetInteger("status-code", violation_data->statusCode());

  csp_report->SetString("script-sample", violation_data->sample());

  auto report_object = std::make_unique<JSONObject>();
  report_object->SetObject("csp-report", std::move(csp_report));
  String stringified_report = report_object->ToJSONString();

  // Only POST unique reports to the external endpoint; repeated reports add no
  // value on the server side, as they're indistinguishable. Note that we'll
  // fire the DOM event for every violation, as the page has enough context to
  // react in some reasonable way to each violation as it occurs.
  if (ShouldSendViolationReport(stringified_report)) {
    DidSendViolationReport(stringified_report);

    // If we're processing 'frame-ancestors', use the delegate for the
    // |context_frame|'s document to post violation report. Otherwise, use the
    // policy's |delegate_|.
    bool is_frame_ancestors_violation = !!context_frame;
    ContentSecurityPolicyDelegate* relevant_delegate =
        is_frame_ancestors_violation
            ? &context_frame->DomWindow()->GetContentSecurityPolicyDelegate()
            : delegate_.Get();
    DCHECK(relevant_delegate);

    relevant_delegate->PostViolationReport(*violation_data, stringified_report,
                                           is_frame_ancestors_violation,
                                           report_endpoints, use_reporting_api);
  }
}

void ContentSecurityPolicy::ReportMixedContent(
    const KURL& blocked_url,
    RedirectStatus redirect_status) const {
  for (const auto& policy : policies_)
    policy->ReportMixedContent(blocked_url, redirect_status);
}

void ContentSecurityPolicy::ReportReportOnlyInMeta(const String& header) {
  LogToConsole("The report-only Content Security Policy '" + header +
               "' was delivered via a <meta> element, which is disallowed. The "
               "policy has been ignored.");
}

void ContentSecurityPolicy::ReportMetaOutsideHead(const String& header) {
  LogToConsole("The Content Security Policy '" + header +
               "' was delivered via a <meta> element outside the document's "
               "<head>, which is disallowed. The policy has been ignored.");
}

void ContentSecurityPolicy::ReportValueForEmptyDirective(const String& name,
                                                         const String& value) {
  LogToConsole("The Content Security Policy directive '" + name +
               "' should be empty, but was delivered with a value of '" +
               value +
               "'. The directive has been applied, and the value ignored.");
}

void ContentSecurityPolicy::ReportMixedContentReportURI(
    const String& endpoint) {
  LogToConsole("The Content Security Policy directive specifies as endpoint '" +
               endpoint +
               "'. This endpoint will be ignored since it violates the policy "
               "for Mixed Content.");
}

void ContentSecurityPolicy::ReportInvalidInReportOnly(const String& name) {
  LogToConsole("The Content Security Policy directive '" + name +
               "' is ignored when delivered in a report-only policy.");
}

void ContentSecurityPolicy::ReportInvalidDirectiveInMeta(
    const String& directive) {
  LogToConsole(
      "Content Security Policies delivered via a <meta> element may not "
      "contain the " +
      directive + " directive.");
}

void ContentSecurityPolicy::ReportUnsupportedDirective(const String& name) {
  static const char kAllow[] = "allow";
  static const char kOptions[] = "options";
  static const char kPolicyURI[] = "policy-uri";
  static const char kAllowMessage[] =
      "The 'allow' directive has been replaced with 'default-src'. Please use "
      "that directive instead, as 'allow' has no effect.";
  static const char kOptionsMessage[] =
      "The 'options' directive has been replaced with 'unsafe-inline' and "
      "'unsafe-eval' source expressions for the 'script-src' and 'style-src' "
      "directives. Please use those directives instead, as 'options' has no "
      "effect.";
  static const char kPolicyURIMessage[] =
      "The 'policy-uri' directive has been removed from the "
      "specification. Please specify a complete policy via "
      "the Content-Security-Policy header.";

  String message =
      "Unrecognized Content-Security-Policy directive '" + name + "'.\n";
  mojom::ConsoleMessageLevel level = mojom::ConsoleMessageLevel::kError;
  if (EqualIgnoringASCIICase(name, kAllow)) {
    message = kAllowMessage;
  } else if (EqualIgnoringASCIICase(name, kOptions)) {
    message = kOptionsMessage;
  } else if (EqualIgnoringASCIICase(name, kPolicyURI)) {
    message = kPolicyURIMessage;
  } else if (GetDirectiveType(name) != DirectiveType::kUndefined) {
    message = "The Content-Security-Policy directive '" + name +
              "' is implemented behind a flag which is currently disabled.\n";
    level = mojom::ConsoleMessageLevel::kInfo;
  }

  LogToConsole(message, level);
}

void ContentSecurityPolicy::ReportDirectiveAsSourceExpression(
    const String& directive_name,
    const String& source_expression) {
  String message = "The Content Security Policy directive '" + directive_name +
                   "' contains '" + source_expression +
                   "' as a source expression. Did you mean '" + directive_name +
                   " ...; " + source_expression + "...' (note the semicolon)?";
  LogToConsole(message);
}

void ContentSecurityPolicy::ReportDuplicateDirective(const String& name) {
  String message =
      "Ignoring duplicate Content-Security-Policy directive '" + name + "'.\n";
  LogToConsole(message);
}

void ContentSecurityPolicy::ReportInvalidPluginTypes(
    const String& plugin_type) {
  String message;
  if (plugin_type.IsNull())
    message =
        "'plugin-types' Content Security Policy directive is empty; all "
        "plugins will be blocked.\n";
  else if (plugin_type == "'none'")
    message =
        "Invalid plugin type in 'plugin-types' Content Security Policy "
        "directive: '" +
        plugin_type +
        "'. Did you mean to set the object-src directive to 'none'?\n";
  else
    message =
        "Invalid plugin type in 'plugin-types' Content Security Policy "
        "directive: '" +
        plugin_type + "'.\n";
  LogToConsole(message);
}

void ContentSecurityPolicy::ReportInvalidSandboxFlags(
    const String& invalid_flags) {
  LogToConsole(
      "Error while parsing the 'sandbox' Content Security Policy directive: " +
      invalid_flags);
}

void ContentSecurityPolicy::ReportInvalidDirectiveValueCharacter(
    const String& directive_name,
    const String& value) {
  String message = "The value for Content Security Policy directive '" +
                   directive_name + "' contains an invalid character: '" +
                   value +
                   "'. Non-whitespace characters outside ASCII 0x21-0x7E must "
                   "be percent-encoded, as described in RFC 3986, section 2.1: "
                   "http://tools.ietf.org/html/rfc3986#section-2.1.";
  LogToConsole(message);
}

void ContentSecurityPolicy::ReportInvalidPathCharacter(
    const String& directive_name,
    const String& value,
    const char invalid_char) {
  DCHECK(invalid_char == '#' || invalid_char == '?');

  String ignoring =
      "The fragment identifier, including the '#', will be ignored.";
  if (invalid_char == '?')
    ignoring = "The query component, including the '?', will be ignored.";
  String message = "The source list for Content Security Policy directive '" +
                   directive_name +
                   "' contains a source with an invalid path: '" + value +
                   "'. " + ignoring;
  LogToConsole(message);
}

void ContentSecurityPolicy::ReportInvalidSourceExpression(
    const String& directive_name,
    const String& source) {
  String message = "The source list for Content Security Policy directive '" +
                   directive_name + "' contains an invalid source: '" + source +
                   "'. It will be ignored.";
  if (EqualIgnoringASCIICase(source, "'none'"))
    message = message +
              " Note that 'none' has no effect unless it is the only "
              "expression in the source list.";
  LogToConsole(message);
}

void ContentSecurityPolicy::ReportMultipleReportToEndpoints() {
  LogToConsole(
      "The Content Security Policy directive 'report-to' contains more than "
      "one endpoint. Only the first one will be used, the other ones will be "
      "ignored.");
}

void ContentSecurityPolicy::LogToConsole(const String& message,
                                         mojom::ConsoleMessageLevel level) {
  LogToConsole(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity, level, message));
}

mojom::blink::ContentSecurityPolicyViolationType
ContentSecurityPolicy::BuildCSPViolationType(
    ContentSecurityPolicy::ContentSecurityPolicyViolationType violation_type) {
  switch (violation_type) {
    case blink::ContentSecurityPolicy::ContentSecurityPolicyViolationType::
        kEvalViolation:
      return mojom::blink::ContentSecurityPolicyViolationType::kEvalViolation;
    case blink::ContentSecurityPolicy::ContentSecurityPolicyViolationType::
        kInlineViolation:
      return mojom::blink::ContentSecurityPolicyViolationType::kInlineViolation;
    case blink::ContentSecurityPolicy::ContentSecurityPolicyViolationType::
        kTrustedTypesPolicyViolation:
      return mojom::blink::ContentSecurityPolicyViolationType::
          kTrustedTypesPolicyViolation;
    case blink::ContentSecurityPolicy::ContentSecurityPolicyViolationType::
        kTrustedTypesSinkViolation:
      return mojom::blink::ContentSecurityPolicyViolationType::
          kTrustedTypesSinkViolation;
    case blink::ContentSecurityPolicy::ContentSecurityPolicyViolationType::
        kURLViolation:
      return mojom::blink::ContentSecurityPolicyViolationType::kURLViolation;
  }
}

void ContentSecurityPolicy::ReportContentSecurityPolicyIssue(
    const blink::SecurityPolicyViolationEventInit& violation_data,
    ContentSecurityPolicyType header_type,
    ContentSecurityPolicyViolationType violation_type,
    LocalFrame* frame_ancestor,
    Element* element) {
  auto cspDetails = mojom::blink::ContentSecurityPolicyIssueDetails::New();
  cspDetails->is_report_only =
      header_type == ContentSecurityPolicyType::kReport;
  if (violation_type == ContentSecurityPolicyViolationType::kURLViolation ||
      violation_data.violatedDirective() == "frame-ancestors") {
    cspDetails->blocked_url = KURL(violation_data.blockedURI());
  }
  cspDetails->violated_directive = violation_data.violatedDirective();
  cspDetails->content_security_policy_violation_type =
      BuildCSPViolationType(violation_type);
  if (frame_ancestor) {
    auto affected_frame = mojom::blink::AffectedFrame::New();
    affected_frame->frame_id =
        frame_ancestor->GetDevToolsFrameToken().ToString().c_str();
    cspDetails->frame_ancestor = std::move(affected_frame);
  }
  if (violation_data.sourceFile() && violation_data.lineNumber()) {
    auto source_location = network::mojom::blink::SourceLocation::New();
    source_location->url = violation_data.sourceFile();
    // The frontend expects 0-based line numbers.
    source_location->line = violation_data.lineNumber() - 1;
    source_location->column = violation_data.columnNumber();
    cspDetails->source_location = std::move(source_location);
  }
  if (element) {
    cspDetails->violating_node_id = DOMNodeIds::IdForNode(element);
  }

  auto details = mojom::blink::InspectorIssueDetails::New();
  details->csp_issue_details = std::move(cspDetails);

  mojom::blink::InspectorIssueInfoPtr info =
      mojom::blink::InspectorIssueInfo::New(
          mojom::blink::InspectorIssueCode::kContentSecurityPolicyIssue,
          std::move(details));

  if (frame_ancestor)
    frame_ancestor->AddInspectorIssue(std::move(info));
  else if (delegate_)
    delegate_->AddInspectorIssue(std::move(info));
}

void ContentSecurityPolicy::LogToConsole(ConsoleMessage* console_message,
                                         LocalFrame* frame) {
  if (frame)
    frame->DomWindow()->AddConsoleMessage(console_message);
  else if (delegate_)
    delegate_->AddConsoleMessage(console_message);
  else
    console_messages_.push_back(console_message);
}

void ContentSecurityPolicy::ReportBlockedScriptExecutionToInspector(
    const String& directive_text) const {
  if (delegate_)
    delegate_->ReportBlockedScriptExecutionToInspector(directive_text);
}

bool ContentSecurityPolicy::ExperimentalFeaturesEnabled() const {
  return RuntimeEnabledFeatures::
      ExperimentalContentSecurityPolicyFeaturesEnabled();
}

bool ContentSecurityPolicy::ShouldSendCSPHeader(ResourceType type) const {
  for (const auto& policy : policies_) {
    if (policy->ShouldSendCSPHeader(type))
      return true;
  }
  return false;
}

bool ContentSecurityPolicy::UrlMatchesSelf(const KURL& url) const {
  return self_source_->MatchesAsSelf(url);
}

bool ContentSecurityPolicy::ProtocolEqualsSelf(const String& protocol) const {
  return EqualIgnoringASCIICase(protocol, self_protocol_);
}

const String& ContentSecurityPolicy::GetSelfProtocol() const {
  return self_protocol_;
}

// static
bool ContentSecurityPolicy::ShouldBypassMainWorld(
    const ExecutionContext* context) {
  if (!context)
    return false;

  return ShouldBypassMainWorld(context->GetCurrentWorld().get());
}

// static
bool ContentSecurityPolicy::ShouldBypassMainWorld(
    const DOMWrapperWorld* world) {
  if (!world || !world->IsIsolatedWorld())
    return false;

  return IsolatedWorldCSP::Get().HasContentSecurityPolicy(world->GetWorldId());
}

bool ContentSecurityPolicy::ShouldSendViolationReport(
    const String& report) const {
  // Collisions have no security impact, so we can save space by storing only
  // the string's hash rather than the whole report.
  return !violation_reports_sent_.Contains(report.Impl()->GetHash());
}

void ContentSecurityPolicy::DidSendViolationReport(const String& report) {
  violation_reports_sent_.insert(report.Impl()->GetHash());
}

const char* ContentSecurityPolicy::GetDirectiveName(const DirectiveType& type) {
  switch (type) {
    case DirectiveType::kBaseURI:
      return "base-uri";
    case DirectiveType::kBlockAllMixedContent:
      return "block-all-mixed-content";
    case DirectiveType::kChildSrc:
      return "child-src";
    case DirectiveType::kConnectSrc:
      return "connect-src";
    case DirectiveType::kDefaultSrc:
      return "default-src";
    case DirectiveType::kFontSrc:
      return "font-src";
    case DirectiveType::kFormAction:
      return "form-action";
    case DirectiveType::kFrameAncestors:
      return "frame-ancestors";
    case DirectiveType::kFrameSrc:
      return "frame-src";
    case DirectiveType::kImgSrc:
      return "img-src";
    case DirectiveType::kManifestSrc:
      return "manifest-src";
    case DirectiveType::kMediaSrc:
      return "media-src";
    case DirectiveType::kNavigateTo:
      return "navigate-to";
    case DirectiveType::kObjectSrc:
      return "object-src";
    case DirectiveType::kPluginTypes:
      return "plugin-types";
    case DirectiveType::kPrefetchSrc:
      return "prefetch-src";
    case DirectiveType::kReportTo:
      return "report-to";
    case DirectiveType::kReportURI:
      return "report-uri";
    case DirectiveType::kRequireTrustedTypesFor:
      return "require-trusted-types-for";
    case DirectiveType::kSandbox:
      return "sandbox";
    case DirectiveType::kScriptSrc:
      return "script-src";
    case DirectiveType::kScriptSrcAttr:
      return "script-src-attr";
    case DirectiveType::kScriptSrcElem:
      return "script-src-elem";
    case DirectiveType::kStyleSrc:
      return "style-src";
    case DirectiveType::kStyleSrcAttr:
      return "style-src-attr";
    case DirectiveType::kStyleSrcElem:
      return "style-src-elem";
    case DirectiveType::kTreatAsPublicAddress:
      return "treat-as-public-address";
    case DirectiveType::kTrustedTypes:
      return "trusted-types";
    case DirectiveType::kUpgradeInsecureRequests:
      return "upgrade-insecure-requests";
    case DirectiveType::kWorkerSrc:
      return "worker-src";

    case DirectiveType::kUndefined:
      NOTREACHED();
      return "";
  }

  NOTREACHED();
  return "";
}

ContentSecurityPolicy::DirectiveType ContentSecurityPolicy::GetDirectiveType(
    const String& name) {
  if (name == "base-uri")
    return DirectiveType::kBaseURI;
  if (name == "block-all-mixed-content")
    return DirectiveType::kBlockAllMixedContent;
  if (name == "child-src")
    return DirectiveType::kChildSrc;
  if (name == "connect-src")
    return DirectiveType::kConnectSrc;
  if (name == "default-src")
    return DirectiveType::kDefaultSrc;
  if (name == "font-src")
    return DirectiveType::kFontSrc;
  if (name == "form-action")
    return DirectiveType::kFormAction;
  if (name == "frame-ancestors")
    return DirectiveType::kFrameAncestors;
  if (name == "frame-src")
    return DirectiveType::kFrameSrc;
  if (name == "img-src")
    return DirectiveType::kImgSrc;
  if (name == "manifest-src")
    return DirectiveType::kManifestSrc;
  if (name == "media-src")
    return DirectiveType::kMediaSrc;
  if (name == "navigate-to")
    return DirectiveType::kNavigateTo;
  if (name == "object-src")
    return DirectiveType::kObjectSrc;
  if (name == "plugin-types")
    return DirectiveType::kPluginTypes;
  if (name == "prefetch-src")
    return DirectiveType::kPrefetchSrc;
  if (name == "report-to")
    return DirectiveType::kReportTo;
  if (name == "report-uri")
    return DirectiveType::kReportURI;
  if (name == "require-trusted-types-for")
    return DirectiveType::kRequireTrustedTypesFor;
  if (name == "sandbox")
    return DirectiveType::kSandbox;
  if (name == "script-src")
    return DirectiveType::kScriptSrc;
  if (name == "script-src-attr")
    return DirectiveType::kScriptSrcAttr;
  if (name == "script-src-elem")
    return DirectiveType::kScriptSrcElem;
  if (name == "style-src")
    return DirectiveType::kStyleSrc;
  if (name == "style-src-attr")
    return DirectiveType::kStyleSrcAttr;
  if (name == "style-src-elem")
    return DirectiveType::kStyleSrcElem;
  if (name == "treat-as-public-address")
    return DirectiveType::kTreatAsPublicAddress;
  if (name == "trusted-types")
    return DirectiveType::kTrustedTypes;
  if (name == "upgrade-insecure-requests")
    return DirectiveType::kUpgradeInsecureRequests;
  if (name == "worker-src")
    return DirectiveType::kWorkerSrc;

  return DirectiveType::kUndefined;
}

bool ContentSecurityPolicy::ShouldBypassContentSecurityPolicy(
    const KURL& url,
    SchemeRegistry::PolicyAreas area) const {
  bool should_bypass_csp;
  if (SecurityOrigin::ShouldUseInnerURL(url)) {
    should_bypass_csp = SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(
        SecurityOrigin::ExtractInnerURL(url).Protocol(), area);
    if (should_bypass_csp) {
      Count(WebFeature::kInnerSchemeBypassesCSP);
    }
  } else {
    should_bypass_csp = SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(
        url.Protocol(), area);
  }
  if (should_bypass_csp) {
    Count(WebFeature::kSchemeBypassesCSP);
  }

  return should_bypass_csp;
}

WTF::Vector<network::mojom::blink::ContentSecurityPolicyPtr>
ContentSecurityPolicy::ExposeForNavigationalChecks() const {
  WTF::Vector<network::mojom::blink::ContentSecurityPolicyPtr> list;
  for (const auto& policy : policies_)
    list.push_back(policy->ExposeForNavigationalChecks());
  return list;
}

bool ContentSecurityPolicy::HasPolicyFromSource(
    ContentSecurityPolicySource source) const {
  for (const auto& policy : policies_) {
    if (policy->HeaderSource() == source)
      return true;
  }
  return false;
}

void ContentSecurityPolicy::Count(WebFeature feature) const {
  if (delegate_)
    delegate_->Count(feature);
}

}  // namespace blink
