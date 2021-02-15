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

int32_t HashAlgorithmsUsed(
    const network::mojom::blink::CSPSourceList* source_list) {
  int32_t hash_algorithms_used = 0;
  if (!source_list)
    return hash_algorithms_used;
  for (const auto& hash : source_list->hashes) {
    hash_algorithms_used |= static_cast<int32_t>(hash->algorithm);
  }
  return hash_algorithms_used;
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

void ContentSecurityPolicy::ApplyPolicySideEffectsToDelegate() {
  DCHECK(delegate_);

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
    Count(GetUseCounterHelperType(policy->header->type));
    if (CSPDirectiveListAllowDynamic(*policy,
                                     CSPDirectiveName::ScriptSrcAttr) ||
        CSPDirectiveListAllowDynamic(*policy,
                                     CSPDirectiveName::ScriptSrcElem)) {
      Count(WebFeature::kCSPWithStrictDynamic);
    }

    if (CSPDirectiveListAllowEval(*policy, this,
                                  ReportingDisposition::kSuppressReporting,
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
    if (CSPDirectiveListIsObjectRestrictionReasonable(*policy)) {
      Count(policy->header->type == ContentSecurityPolicyType::kEnforce
                ? WebFeature::kCSPWithReasonableObjectRestrictions
                : WebFeature::kCSPROWithReasonableObjectRestrictions);
    }
    if (CSPDirectiveListIsBaseRestrictionReasonable(*policy)) {
      Count(policy->header->type == ContentSecurityPolicyType::kEnforce
                ? WebFeature::kCSPWithReasonableBaseRestrictions
                : WebFeature::kCSPROWithReasonableBaseRestrictions);
    }
    if (CSPDirectiveListIsScriptRestrictionReasonable(*policy)) {
      Count(policy->header->type == ContentSecurityPolicyType::kEnforce
                ? WebFeature::kCSPWithReasonableScriptRestrictions
                : WebFeature::kCSPROWithReasonableScriptRestrictions);
    }
    if (CSPDirectiveListIsObjectRestrictionReasonable(*policy) &&
        CSPDirectiveListIsBaseRestrictionReasonable(*policy) &&
        CSPDirectiveListIsScriptRestrictionReasonable(*policy)) {
      Count(policy->header->type == ContentSecurityPolicyType::kEnforce
                ? WebFeature::kCSPWithReasonableRestrictions
                : WebFeature::kCSPROWithReasonableRestrictions);

      if (!CSPDirectiveListAllowDynamic(*policy,
                                        CSPDirectiveName::ScriptSrcElem)) {
        Count(policy->header->type == ContentSecurityPolicyType::kEnforce
                  ? WebFeature::kCSPWithBetterThanReasonableRestrictions
                  : WebFeature::kCSPROWithBetterThanReasonableRestrictions);
      }
    }
    if (CSPDirectiveListRequiresTrustedTypes(*policy)) {
      Count(CSPDirectiveListIsReportOnly(*policy)
                ? WebFeature::kTrustedTypesEnabledReportOnly
                : WebFeature::kTrustedTypesEnabledEnforcing);
    }
    if (policy->trusted_types && policy->trusted_types->allow_duplicates) {
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
  visitor->Trace(console_messages_);
}

void ContentSecurityPolicy::CopyStateFrom(const ContentSecurityPolicy* other) {
  DCHECK(policies_.IsEmpty());
  policies_ = mojo::Clone(other->policies_);
  for (const auto& policy : policies_)
    ComputeInternalStateForParsedPolicy(*policy);
  if (delegate_)
    ReportAccumulatedHeaders();
}

void ContentSecurityPolicy::DidReceiveHeaders(
    const ContentSecurityPolicyResponseHeaders& headers) {
  scoped_refptr<SecurityOrigin> self_origin =
      SecurityOrigin::Create(headers.ResponseUrl());
  if (headers.ShouldParseWasmEval())
    supports_wasm_eval_ = true;

  Vector<network::mojom::blink::ContentSecurityPolicyPtr> parsed_policies;
  if (!headers.ContentSecurityPolicy().IsEmpty()) {
    parsed_policies = Parse(headers.ContentSecurityPolicy(), *self_origin,
                            ContentSecurityPolicyType::kEnforce,
                            ContentSecurityPolicySource::kHTTP);
  }
  if (!headers.ContentSecurityPolicyReportOnly().IsEmpty()) {
    for (auto& policy : Parse(headers.ContentSecurityPolicyReportOnly(),
                              *self_origin, ContentSecurityPolicyType::kReport,
                              ContentSecurityPolicySource::kHTTP)) {
      parsed_policies.push_back(std::move(policy));
    }
  }

  AddPolicies(std::move(parsed_policies));
}

// static
WTF::Vector<network::mojom::blink::ContentSecurityPolicyPtr>
ContentSecurityPolicy::ParseHeaders(
    const ContentSecurityPolicyResponseHeaders& headers) {
  auto* content_security_policy = MakeGarbageCollected<ContentSecurityPolicy>();
  content_security_policy->DidReceiveHeaders(headers);
  return std::move(content_security_policy->policies_);
}

void ContentSecurityPolicy::DidReceiveHeader(
    const String& header,
    const SecurityOrigin& self_origin,
    ContentSecurityPolicyType type,
    ContentSecurityPolicySource source) {
  AddPolicies(Parse(header, self_origin, type, source));
}

void ContentSecurityPolicy::AddPolicies(
    Vector<network::mojom::blink::ContentSecurityPolicyPtr> policies) {
  Vector<network::mojom::blink::ContentSecurityPolicyPtr> policies_to_report;
  if (delegate_) {
    policies_to_report = mojo::Clone(policies);
  }

  for (network::mojom::blink::ContentSecurityPolicyPtr& policy : policies) {
    ComputeInternalStateForParsedPolicy(*policy);
    policies_.push_back(std::move(policy));
  }

  if (!delegate_)
    return;

  ApplyPolicySideEffectsToDelegate();

  // Notify about the new header, so that it can be reported back to the
  // browser process. This is needed in order to:
  // 1) replicate CSP directives (i.e. frame-src) to OOPIFs (only for now /
  // short-term).
  // 2) enforce CSP in the browser process (long-term - see
  // https://crbug.com/376522).
  // TODO(arthursonzogni): policies are actually replicated (1) and some of
  // them are enforced on the browser process (2). Stop doing (1) when (2) is
  // finished.
  delegate_->DidAddContentSecurityPolicies(std::move(policies_to_report));
}

Vector<network::mojom::blink::ContentSecurityPolicyPtr>
ContentSecurityPolicy::Parse(const String& header,
                             const SecurityOrigin& self_origin,
                             ContentSecurityPolicyType type,
                             ContentSecurityPolicySource source) {
  Vector<network::mojom::blink::ContentSecurityPolicyPtr> policies;

  // If this is a report-only header inside a <meta> element, bail out.
  if (source == ContentSecurityPolicySource::kMeta &&
      type == ContentSecurityPolicyType::kReport) {
    ReportReportOnlyInMeta(header);
    return policies;
  }

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
    policies.push_back(CSPDirectiveListParse(this, begin, position, self_origin,
                                             type, source));

    // Skip the comma, and begin the next header from the current position.
    DCHECK(position == end || *position == ',');
    SkipExactly<UChar>(position, end, ',');
    begin = position;
  }
  return policies;
}

void ContentSecurityPolicy::ComputeInternalStateForParsedPolicy(
    const network::mojom::blink::ContentSecurityPolicy& csp) {
  if (csp.header->source == ContentSecurityPolicySource::kHTTP)
    header_delivered_ = true;

  if (csp.block_all_mixed_content && !CSPDirectiveListIsReportOnly(csp))
    EnforceStrictMixedContentChecking();

  if (CSPDirectiveListRequiresTrustedTypes(csp))
    RequireTrustedTypes();

  EnforceSandboxFlags(csp.sandbox);

  if (csp.upgrade_insecure_requests)
    UpgradeInsecureRequests();

  String disable_eval_message;
  if (CSPDirectiveListShouldDisableEval(csp, disable_eval_message) &&
      disable_eval_error_message_.IsNull()) {
    disable_eval_error_message_ = disable_eval_message;
  }

  for (const auto& directive : csp.directives) {
    switch (directive.key) {
      case CSPDirectiveName::DefaultSrc:
        // TODO(mkwst) It seems unlikely that developers would use different
        // algorithms for scripts and styles. We may want to combine the
        // usesScriptHashAlgorithms() and usesStyleHashAlgorithms.
        UsesScriptHashAlgorithms(HashAlgorithmsUsed(directive.value.get()));
        UsesStyleHashAlgorithms(HashAlgorithmsUsed(directive.value.get()));
        break;
      case CSPDirectiveName::ScriptSrc:
      case CSPDirectiveName::ScriptSrcAttr:
      case CSPDirectiveName::ScriptSrcElem:
        UsesScriptHashAlgorithms(HashAlgorithmsUsed(directive.value.get()));
        break;
      case CSPDirectiveName::StyleSrc:
      case CSPDirectiveName::StyleSrcAttr:
      case CSPDirectiveName::StyleSrcElem:
        UsesStyleHashAlgorithms(HashAlgorithmsUsed(directive.value.get()));
        break;
      default:
        break;
    }
  }
}

void ContentSecurityPolicy::ReportAccumulatedHeaders() const {
  DCHECK(delegate_);
  delegate_->DidAddContentSecurityPolicies(mojo::Clone(GetParsedPolicies()));
}

void ContentSecurityPolicy::SetOverrideAllowInlineStyle(bool value) {
  override_inline_style_allowed_ = value;
}

// static
void ContentSecurityPolicy::FillInCSPHashValues(
    const String& source,
    uint8_t hash_algorithms_used,
    Vector<network::mojom::blink::CSPHashSourcePtr>& csp_hash_values) {
  // Any additions or subtractions from this struct should also modify the
  // respective entries in the kSupportedPrefixes array in
  // SourceListDirective::parseHash().
  static const struct {
    network::mojom::blink::CSPHashAlgorithm csp_hash_algorithm;
    HashAlgorithm algorithm;
  } kAlgorithmMap[] = {
      {network::mojom::blink::CSPHashAlgorithm::SHA256, kHashAlgorithmSha256},
      {network::mojom::blink::CSPHashAlgorithm::SHA384, kHashAlgorithmSha384},
      {network::mojom::blink::CSPHashAlgorithm::SHA512, kHashAlgorithmSha512}};

  // Only bother normalizing the source/computing digests if there are any
  // checks to be done.
  if (hash_algorithms_used == kContentSecurityPolicyHashAlgorithmNone)
    return;

  StringUTF8Adaptor utf8_source(
      source, kStrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD);

  for (const auto& algorithm_map : kAlgorithmMap) {
    DigestValue digest;
    if (static_cast<int32_t>(algorithm_map.csp_hash_algorithm) &
        hash_algorithms_used) {
      bool digest_success =
          ComputeDigest(algorithm_map.algorithm, utf8_source.data(),
                        utf8_source.size(), digest);
      if (digest_success) {
        csp_hash_values.push_back(network::mojom::blink::CSPHashSource::New(
            algorithm_map.csp_hash_algorithm, Vector<uint8_t>(digest)));
      }
    }
  }
}

// static
bool ContentSecurityPolicy::CheckHashAgainstPolicy(
    Vector<network::mojom::blink::CSPHashSourcePtr>& csp_hash_values,
    const network::mojom::blink::ContentSecurityPolicy& csp,
    InlineType inline_type) {
  for (const auto& csp_hash_value : csp_hash_values) {
    if (CSPDirectiveListAllowHash(csp, *csp_hash_value, inline_type))
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
    ReportingDisposition reporting_disposition) {
  DCHECK(element || inline_type == InlineType::kScriptAttribute ||
         inline_type == InlineType::kNavigation);

  const bool is_script = IsScriptInlineType(inline_type);
  if (!is_script && override_inline_style_allowed_) {
    return true;
  }

  Vector<network::mojom::blink::CSPHashSourcePtr> csp_hash_values;
  FillInCSPHashValues(
      content,
      is_script ? script_hash_algorithms_used_ : style_hash_algorithms_used_,
      csp_hash_values);

  // Step 2. Let result be "Allowed". [spec text]
  bool is_allowed = true;

  // Step 3. For each policy in element’s Document's global object’s CSP list:
  // [spec text]
  for (const auto& policy : policies_) {
    // May be allowed by hash, if 'unsafe-hashes' is present in a policy.
    // Check against the digest of the |content| and also check whether inline
    // script is allowed.
    is_allowed &=
        CheckHashAgainstPolicy(csp_hash_values, *policy, inline_type) ||
        CSPDirectiveListAllowInline(*policy, this, inline_type, element,
                                    content, nonce, context_url, context_line,
                                    reporting_disposition);
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
    if (CSPDirectiveListShouldCheckEval(*policy))
      return true;
  }
  return IsRequireTrustedTypes();
}

bool ContentSecurityPolicy::AllowEval(
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& script_content) {
  bool is_allowed = true;
  for (const auto& policy : policies_) {
    is_allowed &= CSPDirectiveListAllowEval(
        *policy, this, reporting_disposition, exception_status, script_content);
  }
  return is_allowed;
}

bool ContentSecurityPolicy::AllowWasmEval(
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& script_content) {
  bool is_allowed = true;
  for (const auto& policy : policies_) {
    is_allowed &= CSPDirectiveListAllowWasmEval(
        *policy, this, reporting_disposition, exception_status, script_content);
  }
  return is_allowed;
}

String ContentSecurityPolicy::EvalDisabledErrorMessage() const {
  for (const auto& policy : policies_) {
    String message;
    if (CSPDirectiveListShouldDisableEval(*policy, message))
      return message;
  }
  return String();
}

static base::Optional<CSPDirectiveName> GetDirectiveTypeFromRequestContextType(
    mojom::blink::RequestContextType context) {
  switch (context) {
    case mojom::blink::RequestContextType::AUDIO:
    case mojom::blink::RequestContextType::TRACK:
    case mojom::blink::RequestContextType::VIDEO:
      return CSPDirectiveName::MediaSrc;

    case mojom::blink::RequestContextType::BEACON:
    case mojom::blink::RequestContextType::EVENT_SOURCE:
    case mojom::blink::RequestContextType::FETCH:
    case mojom::blink::RequestContextType::PING:
    case mojom::blink::RequestContextType::XML_HTTP_REQUEST:
    case mojom::blink::RequestContextType::SUBRESOURCE:
    case mojom::blink::RequestContextType::SUBRESOURCE_WEBBUNDLE:
      return CSPDirectiveName::ConnectSrc;

    case mojom::blink::RequestContextType::EMBED:
    case mojom::blink::RequestContextType::OBJECT:
      return CSPDirectiveName::ObjectSrc;

    case mojom::blink::RequestContextType::PREFETCH:
      return CSPDirectiveName::PrefetchSrc;

    case mojom::blink::RequestContextType::FAVICON:
    case mojom::blink::RequestContextType::IMAGE:
    case mojom::blink::RequestContextType::IMAGE_SET:
      return CSPDirectiveName::ImgSrc;

    case mojom::blink::RequestContextType::FONT:
      return CSPDirectiveName::FontSrc;

    case mojom::blink::RequestContextType::FORM:
      return CSPDirectiveName::FormAction;

    case mojom::blink::RequestContextType::FRAME:
    case mojom::blink::RequestContextType::IFRAME:
      return CSPDirectiveName::FrameSrc;

    case mojom::blink::RequestContextType::IMPORT:
    case mojom::blink::RequestContextType::SCRIPT:
    case mojom::blink::RequestContextType::XSLT:
      return CSPDirectiveName::ScriptSrcElem;

    case mojom::blink::RequestContextType::MANIFEST:
      return CSPDirectiveName::ManifestSrc;

    case mojom::blink::RequestContextType::SERVICE_WORKER:
    case mojom::blink::RequestContextType::SHARED_WORKER:
    case mojom::blink::RequestContextType::WORKER:
      return CSPDirectiveName::WorkerSrc;

    case mojom::blink::RequestContextType::STYLE:
      return CSPDirectiveName::StyleSrcElem;

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
    CheckHeaderType check_header_type) {
  base::Optional<CSPDirectiveName> type =
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
    CSPDirectiveName type,
    const KURL& url,
    const KURL& url_before_redirects,
    RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    CheckHeaderType check_header_type,
    const String& nonce,
    const IntegrityMetadataSet& hashes,
    ParserDisposition parser_disposition) {
  SchemeRegistry::PolicyAreas area = SchemeRegistry::kPolicyAreaAll;
  if (type == CSPDirectiveName::ImgSrc)
    area = SchemeRegistry::kPolicyAreaImage;
  else if (type == CSPDirectiveName::StyleSrcElem)
    area = SchemeRegistry::kPolicyAreaStyle;

  if (ShouldBypassContentSecurityPolicy(url, area)) {
    if (type != CSPDirectiveName::ScriptSrcElem)
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
    if (!CheckHeaderTypeMatches(check_header_type, policy->header->type))
      continue;
    is_allowed &= CSPDirectiveListAllowFromSource(
        *policy, this, type, url, url_before_redirects, redirect_status,
        reporting_disposition, nonce, hashes, parser_disposition);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowBaseURI(const KURL& url) {
  // `base-uri` isn't affected by 'upgrade-insecure-requests', so we use
  // CheckHeaderType::kCheckAll to check both report-only and enforce headers
  // here.
  return AllowFromSource(CSPDirectiveName::BaseURI, url, url,
                         RedirectStatus::kNoRedirect);
}

bool ContentSecurityPolicy::AllowConnectToSource(
    const KURL& url,
    const KURL& url_before_redirects,
    RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    CheckHeaderType check_header_type) {
  return AllowFromSource(CSPDirectiveName::ConnectSrc, url,
                         url_before_redirects, redirect_status,
                         reporting_disposition, check_header_type);
}

bool ContentSecurityPolicy::AllowFormAction(const KURL& url) {
  return AllowFromSource(CSPDirectiveName::FormAction, url, url,
                         RedirectStatus::kNoRedirect);
}

bool ContentSecurityPolicy::AllowImageFromSource(
    const KURL& url,
    const KURL& url_before_redirects,
    RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    CheckHeaderType check_header_type) {
  return AllowFromSource(CSPDirectiveName::ImgSrc, url, url_before_redirects,
                         redirect_status, reporting_disposition,
                         check_header_type);
}

bool ContentSecurityPolicy::AllowMediaFromSource(const KURL& url) {
  return AllowFromSource(CSPDirectiveName::MediaSrc, url, url,
                         RedirectStatus::kNoRedirect);
}

bool ContentSecurityPolicy::AllowObjectFromSource(const KURL& url) {
  return AllowFromSource(CSPDirectiveName::ObjectSrc, url, url,
                         RedirectStatus::kNoRedirect);
}

bool ContentSecurityPolicy::AllowScriptFromSource(
    const KURL& url,
    const String& nonce,
    const IntegrityMetadataSet& hashes,
    ParserDisposition parser_disposition,
    const KURL& url_before_redirects,
    RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    CheckHeaderType check_header_type) {
  return AllowFromSource(CSPDirectiveName::ScriptSrcElem, url,
                         url_before_redirects, redirect_status,
                         reporting_disposition, check_header_type, nonce,
                         hashes, parser_disposition);
}

bool ContentSecurityPolicy::AllowWorkerContextFromSource(const KURL& url) {
  return AllowFromSource(CSPDirectiveName::WorkerSrc, url, url,
                         RedirectStatus::kNoRedirect);
}

// The return value indicates whether the policy is allowed or not.
// If the return value is false, the out-parameter violation_details indicates
// the type of the violation, and if the return value is true,
// it indicates if a report-only violation occurred.
bool ContentSecurityPolicy::AllowTrustedTypePolicy(
    const String& policy_name,
    bool is_duplicate,
    AllowTrustedTypePolicyDetails& violation_details) {
  bool is_allowed = true;
  violation_details = AllowTrustedTypePolicyDetails::kAllowed;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(CheckHeaderType::kCheckAll,
                                policy->header->type)) {
      continue;
    }
    auto new_violation_details = AllowTrustedTypePolicyDetails::kAllowed;
    bool new_allowed = CSPDirectiveListAllowTrustedTypePolicy(
        *policy, this, policy_name, is_duplicate, new_violation_details);
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
    const String& sample_prefix) {
  bool allow = true;
  for (const auto& policy : policies_) {
    allow &= CSPDirectiveListAllowTrustedTypeAssignmentFailure(
        *policy, this, message, sample, sample_prefix);
  }
  return allow;
}

bool ContentSecurityPolicy::IsActive() const {
  return !policies_.IsEmpty();
}

bool ContentSecurityPolicy::IsActiveForConnections() const {
  for (const auto& policy : policies_) {
    if (CSPDirectiveListIsActiveForConnections(*policy))
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

static String StripURLForUseInReport(const SecurityOrigin* security_origin,
                                     const KURL& url,
                                     RedirectStatus redirect_status,
                                     CSPDirectiveName effective_type) {
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
       effective_type != CSPDirectiveName::FrameSrc &&
       effective_type != CSPDirectiveName::ObjectSrc);

  if (can_safely_expose_url) {
    // 'KURL::strippedForUseAsReferrer()' dumps 'String()' for non-webby URLs.
    // It's better for developers if we return the origin of those URLs rather
    // than nothing.
    if (url.ProtocolIsInHTTPFamily())
      return url.StrippedForUseAsReferrer();
  }
  return SecurityOrigin::Create(url)->ToString();
}

namespace {
std::unique_ptr<SourceLocation> GatherSecurityPolicyViolationEventData(
    SecurityPolicyViolationEventInit* init,
    ContentSecurityPolicyDelegate* delegate,
    const String& directive_text,
    CSPDirectiveName effective_type,
    const KURL& blocked_url,
    const String& header,
    RedirectStatus redirect_status,
    ContentSecurityPolicyType header_type,
    ContentSecurityPolicy::ContentSecurityPolicyViolationType violation_type,
    std::unique_ptr<SourceLocation> source_location,
    const String& script_source,
    const String& sample_prefix) {
  if (effective_type == CSPDirectiveName::FrameAncestors) {
    // If this load was blocked via 'frame-ancestors', then the URL of
    // |document| has not yet been initialized. In this case, we'll set both
    // 'documentURI' and 'blockedURI' to the blocked document's URL.
    String stripped_url = StripURLForUseInReport(
        delegate->GetSecurityOrigin(), blocked_url, RedirectStatus::kNoRedirect,
        CSPDirectiveName::DefaultSrc);
    init->setDocumentURI(stripped_url);
    init->setBlockedURI(stripped_url);
  } else {
    String stripped_url = StripURLForUseInReport(
        delegate->GetSecurityOrigin(), delegate->Url(),
        RedirectStatus::kNoRedirect, CSPDirectiveName::DefaultSrc);
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

  return source_location;
}
}  // namespace

void ContentSecurityPolicy::ReportViolation(
    const String& directive_text,
    CSPDirectiveName effective_type,
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
    DCHECK(effective_type == CSPDirectiveName::ChildSrc ||
           effective_type == CSPDirectiveName::FrameSrc ||
           effective_type == CSPDirectiveName::TrustedTypes ||
           effective_type == CSPDirectiveName::RequireTrustedTypesFor);
    return;
  }
  DCHECK(
      (delegate_ && !context_frame) ||
      ((effective_type == CSPDirectiveName::FrameAncestors) && context_frame));

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
  // Let GatherSecurityPolicyViolationEventData decide which source location to
  // report.
  source_location = GatherSecurityPolicyViolationEventData(
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
                                   context_frame, element,
                                   source_location.get());
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

void ContentSecurityPolicy::ReportMixedContent(const KURL& blocked_url,
                                               RedirectStatus redirect_status) {
  for (const auto& policy : policies_) {
    if (policy->block_all_mixed_content) {
      ReportViolation(GetDirectiveName(CSPDirectiveName::BlockAllMixedContent),
                      CSPDirectiveName::BlockAllMixedContent, String(),
                      blocked_url, policy->report_endpoints,
                      policy->use_reporting_api, policy->header->header_value,
                      policy->header->type,
                      ContentSecurityPolicy::kURLViolation,
                      std::unique_ptr<SourceLocation>(),
                      /*contextFrame=*/nullptr, redirect_status);
    }
  }
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
  static const char kPluginTypes[] = "plugin-types";
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
  static const char kPluginTypesMessage[] =
      "The Content-Security-Policy directive 'plugin-types' has been removed "
      "from the specification. "
      "If you want to block plugins, consider specifying \"object-src 'none'\" "
      "instead.";

  String message =
      "Unrecognized Content-Security-Policy directive '" + name + "'.\n";
  mojom::ConsoleMessageLevel level = mojom::ConsoleMessageLevel::kError;
  if (EqualIgnoringASCIICase(name, kAllow)) {
    message = kAllowMessage;
  } else if (EqualIgnoringASCIICase(name, kOptions)) {
    message = kOptionsMessage;
  } else if (EqualIgnoringASCIICase(name, kPolicyURI)) {
    message = kPolicyURIMessage;
  } else if (EqualIgnoringASCIICase(name, kPluginTypes)) {
    message = kPluginTypesMessage;
  } else if (GetDirectiveType(name) != CSPDirectiveName::Unknown) {
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

void ContentSecurityPolicy::ReportInvalidRequireTrustedTypesFor(
    const String& require_trusted_types_for) {
  String message;
  if (require_trusted_types_for.IsNull()) {
    message =
        "'require-trusted-types-for' Content Security Policy directive is "
        "empty; The directive has no effect.\n";
  } else {
    const char* hint = "";
    if (require_trusted_types_for == "script" ||
        require_trusted_types_for == "scripts" ||
        require_trusted_types_for == "'scripts'") {
      hint = " Did you mean 'script'?";
    }

    message =
        "Invalid expression in 'require-trusted-types-for' "
        "Content Security Policy directive: " +
        require_trusted_types_for + "." + hint + "\n";
  }
  LogToConsole(message, mojom::ConsoleMessageLevel::kWarning);
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
  String message =
      "The value for Content Security Policy directive '" + directive_name +
      "' contains an invalid character: '" + value +
      "'. In a source expression, non-whitespace characters outside ASCII "
      "0x21-0x7E must be Punycode-encoded, as described in RFC 3492 "
      "(https://tools.ietf.org/html/rfc3492), if part of the hostname and  "
      "percent-encoded, as described in RFC 3986, section 2.1 "
      "(http://tools.ietf.org/html/rfc3986#section-2.1), if part of the path.";
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
    Element* element,
    SourceLocation* source_location) {
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
    auto affected_location = mojom::blink::AffectedLocation::New();
    affected_location->url = violation_data.sourceFile();
    // The frontend expects 0-based line numbers.
    affected_location->line = violation_data.lineNumber() - 1;
    affected_location->column = violation_data.columnNumber();
    if (source_location) {
      affected_location->script_id =
          String::Number(source_location->ScriptId());
    }
    cspDetails->affected_location = std::move(affected_location);
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
  // TODO(mkwst): Revisit this once the CORS prefetch issue with the 'CSP'
  //              header is worked out, one way or another:
  //              https://github.com/whatwg/fetch/issues/52
  return false;
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

const char* ContentSecurityPolicy::GetDirectiveName(CSPDirectiveName type) {
  switch (type) {
    case CSPDirectiveName::BaseURI:
      return "base-uri";
    case CSPDirectiveName::BlockAllMixedContent:
      return "block-all-mixed-content";
    case CSPDirectiveName::ChildSrc:
      return "child-src";
    case CSPDirectiveName::ConnectSrc:
      return "connect-src";
    case CSPDirectiveName::DefaultSrc:
      return "default-src";
    case CSPDirectiveName::FontSrc:
      return "font-src";
    case CSPDirectiveName::FormAction:
      return "form-action";
    case CSPDirectiveName::FrameAncestors:
      return "frame-ancestors";
    case CSPDirectiveName::FrameSrc:
      return "frame-src";
    case CSPDirectiveName::ImgSrc:
      return "img-src";
    case CSPDirectiveName::ManifestSrc:
      return "manifest-src";
    case CSPDirectiveName::MediaSrc:
      return "media-src";
    case CSPDirectiveName::NavigateTo:
      return "navigate-to";
    case CSPDirectiveName::ObjectSrc:
      return "object-src";
    case CSPDirectiveName::PrefetchSrc:
      return "prefetch-src";
    case CSPDirectiveName::ReportTo:
      return "report-to";
    case CSPDirectiveName::ReportURI:
      return "report-uri";
    case CSPDirectiveName::RequireTrustedTypesFor:
      return "require-trusted-types-for";
    case CSPDirectiveName::Sandbox:
      return "sandbox";
    case CSPDirectiveName::ScriptSrc:
      return "script-src";
    case CSPDirectiveName::ScriptSrcAttr:
      return "script-src-attr";
    case CSPDirectiveName::ScriptSrcElem:
      return "script-src-elem";
    case CSPDirectiveName::StyleSrc:
      return "style-src";
    case CSPDirectiveName::StyleSrcAttr:
      return "style-src-attr";
    case CSPDirectiveName::StyleSrcElem:
      return "style-src-elem";
    case CSPDirectiveName::TreatAsPublicAddress:
      return "treat-as-public-address";
    case CSPDirectiveName::TrustedTypes:
      return "trusted-types";
    case CSPDirectiveName::UpgradeInsecureRequests:
      return "upgrade-insecure-requests";
    case CSPDirectiveName::WorkerSrc:
      return "worker-src";

    case CSPDirectiveName::Unknown:
      NOTREACHED();
      return "";
  }

  NOTREACHED();
  return "";
}

CSPDirectiveName ContentSecurityPolicy::GetDirectiveType(const String& name) {
  if (name == "base-uri")
    return CSPDirectiveName::BaseURI;
  if (name == "block-all-mixed-content")
    return CSPDirectiveName::BlockAllMixedContent;
  if (name == "child-src")
    return CSPDirectiveName::ChildSrc;
  if (name == "connect-src")
    return CSPDirectiveName::ConnectSrc;
  if (name == "default-src")
    return CSPDirectiveName::DefaultSrc;
  if (name == "font-src")
    return CSPDirectiveName::FontSrc;
  if (name == "form-action")
    return CSPDirectiveName::FormAction;
  if (name == "frame-ancestors")
    return CSPDirectiveName::FrameAncestors;
  if (name == "frame-src")
    return CSPDirectiveName::FrameSrc;
  if (name == "img-src")
    return CSPDirectiveName::ImgSrc;
  if (name == "manifest-src")
    return CSPDirectiveName::ManifestSrc;
  if (name == "media-src")
    return CSPDirectiveName::MediaSrc;
  if (name == "navigate-to")
    return CSPDirectiveName::NavigateTo;
  if (name == "object-src")
    return CSPDirectiveName::ObjectSrc;
  if (name == "prefetch-src")
    return CSPDirectiveName::PrefetchSrc;
  if (name == "report-to")
    return CSPDirectiveName::ReportTo;
  if (name == "report-uri")
    return CSPDirectiveName::ReportURI;
  if (name == "require-trusted-types-for")
    return CSPDirectiveName::RequireTrustedTypesFor;
  if (name == "sandbox")
    return CSPDirectiveName::Sandbox;
  if (name == "script-src")
    return CSPDirectiveName::ScriptSrc;
  if (name == "script-src-attr")
    return CSPDirectiveName::ScriptSrcAttr;
  if (name == "script-src-elem")
    return CSPDirectiveName::ScriptSrcElem;
  if (name == "style-src")
    return CSPDirectiveName::StyleSrc;
  if (name == "style-src-attr")
    return CSPDirectiveName::StyleSrcAttr;
  if (name == "style-src-elem")
    return CSPDirectiveName::StyleSrcElem;
  if (name == "treat-as-public-address")
    return CSPDirectiveName::TreatAsPublicAddress;
  if (name == "trusted-types")
    return CSPDirectiveName::TrustedTypes;
  if (name == "upgrade-insecure-requests")
    return CSPDirectiveName::UpgradeInsecureRequests;
  if (name == "worker-src")
    return CSPDirectiveName::WorkerSrc;

  return CSPDirectiveName::Unknown;
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

const WTF::Vector<network::mojom::blink::ContentSecurityPolicyPtr>&
ContentSecurityPolicy::GetParsedPolicies() const {
  return policies_;
}

bool ContentSecurityPolicy::HasPolicyFromSource(
    ContentSecurityPolicySource source) const {
  for (const auto& policy : policies_) {
    if (policy->header->source == source)
      return true;
  }
  return false;
}

void ContentSecurityPolicy::Count(WebFeature feature) const {
  if (delegate_)
    delegate_->Count(feature);
}

}  // namespace blink
