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
#include <vector>

#include "third_party/blink/public/mojom/net/ip_address_space.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_string_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/events/security_policy_violation_event.h"
#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"
#include "third_party/blink/renderer/core/frame/csp/csp_source.h"
#include "third_party/blink/renderer/core/frame/csp/media_list_directive.h"
#include "third_party/blink/renderer/core/frame/csp/source_list_directive.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/sandbox_flags.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/ping_loader.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_response_headers.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_service_proxy_ptr_holder.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/not_found.h"
#include "third_party/blink/renderer/platform/wtf/string_hasher.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

namespace {

// Helper function that returns true if the given |header_type| should be
// checked when the CheckHeaderType is |check_header_type|.
bool CheckHeaderTypeMatches(
    ContentSecurityPolicy::CheckHeaderType check_header_type,
    ContentSecurityPolicyHeaderType header_type) {
  switch (check_header_type) {
    case ContentSecurityPolicy::CheckHeaderType::kCheckAll:
      return true;
    case ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly:
      return header_type == kContentSecurityPolicyHeaderTypeReport;
    case ContentSecurityPolicy::CheckHeaderType::kCheckEnforce:
      return header_type == kContentSecurityPolicyHeaderTypeEnforce;
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
  // TODO(mkwst): We'll should also skip elements for which the HTML parser
  // dropped attributes: https://crbug.com/740615 and https://crbug.com/790955.
  //
  // See http://blog.innerht.ml/csp-2015/#danglingmarkupinjection for an example
  // of the kind of attack this is aimed at mitigating.
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
      element->GetDocument(),
      nonceable ? WebFeature::kCleanScriptElementWithNonce
                : WebFeature::kPotentiallyInjectedScriptElementWithNonce);

  return nonceable;
}

static WebFeature GetUseCounterType(ContentSecurityPolicyHeaderType type) {
  switch (type) {
    case kContentSecurityPolicyHeaderTypeEnforce:
      return WebFeature::kContentSecurityPolicy;
    case kContentSecurityPolicyHeaderTypeReport:
      return WebFeature::kContentSecurityPolicyReportOnly;
  }
  NOTREACHED();
  return WebFeature::kNumberOfFeatures;
}

ContentSecurityPolicy::ContentSecurityPolicy()
    : execution_context_(nullptr),
      override_inline_style_allowed_(false),
      script_hash_algorithms_used_(kContentSecurityPolicyHashAlgorithmNone),
      style_hash_algorithms_used_(kContentSecurityPolicyHashAlgorithmNone),
      sandbox_mask_(0),
      treat_as_public_address_(false),
      require_safe_types_(false),
      insecure_request_policy_(kLeaveInsecureRequestsAlone) {}

void ContentSecurityPolicy::BindToExecutionContext(
    ExecutionContext* execution_context) {
  execution_context_ = execution_context;
  ApplyPolicySideEffectsToExecutionContext();
}

void ContentSecurityPolicy::SetupSelf(const SecurityOrigin& security_origin) {
  // Ensure that 'self' processes correctly.
  self_protocol_ = security_origin.Protocol();
  self_source_ = new CSPSource(this, self_protocol_, security_origin.Host(),
                               security_origin.Port(), String(),
                               CSPSource::kNoWildcard, CSPSource::kNoWildcard);
}

void ContentSecurityPolicy::ApplyPolicySideEffectsToExecutionContext() {
  DCHECK(execution_context_ &&
         execution_context_->GetSecurityContext().GetSecurityOrigin());
  SecurityContext& security_context = execution_context_->GetSecurityContext();

  SetupSelf(*security_context.GetSecurityOrigin());

  // Set mixed content checking and sandbox flags, then dump all the parsing
  // error messages, then poke at histograms.
  Document* document = this->GetDocument();
  if (sandbox_mask_ != kSandboxNone) {
    UseCounter::Count(execution_context_, WebFeature::kSandboxViaCSP);
    if (document)
      document->EnforceSandboxFlags(sandbox_mask_);
    else
      security_context.ApplySandboxFlags(sandbox_mask_);
  }
  if (treat_as_public_address_) {
    security_context.SetAddressSpace(mojom::IPAddressSpace::kPublic);
  }
  if (require_safe_types_) {
    security_context.SetRequireTrustedTypes();
  }

  // Upgrade Insecure Requests: Update the policy.
  security_context.SetInsecureRequestPolicy(
      security_context.GetInsecureRequestPolicy() | insecure_request_policy_);
  if (document)
    document->DidEnforceInsecureRequestPolicy();

  // Upgrade Insecure Requests: Update the set of insecure URLs to upgrade.
  if (insecure_request_policy_ & kUpgradeInsecureRequests) {
    UseCounter::Count(execution_context_,
                      WebFeature::kUpgradeInsecureRequestsEnabled);
    if (!execution_context_->Url().Host().IsEmpty()) {
      uint32_t hash = execution_context_->Url().Host().Impl()->GetHash();
      security_context.AddInsecureNavigationUpgrade(hash);
      if (document)
        document->DidEnforceInsecureNavigationsSet();
    }
  }

  for (const auto& console_message : console_messages_)
    execution_context_->AddConsoleMessage(console_message);
  console_messages_.clear();

  for (const auto& policy : policies_) {
    UseCounter::Count(execution_context_,
                      GetUseCounterType(policy->HeaderType()));
    if (policy->AllowDynamic(
            ContentSecurityPolicy::DirectiveType::kScriptSrcAttr) ||
        policy->AllowDynamic(
            ContentSecurityPolicy::DirectiveType::kScriptSrcElem)) {
      UseCounter::Count(execution_context_, WebFeature::kCSPWithStrictDynamic);
    }

    if (policy->AllowEval(nullptr,
                          SecurityViolationReportingPolicy::kSuppressReporting,
                          kWillNotThrowException, g_empty_string)) {
      UseCounter::Count(execution_context_, WebFeature::kCSPWithUnsafeEval);
    }
  }

  // We disable 'eval()' even in the case of report-only policies, and rely on
  // the check in the V8Initializer::codeGenerationCheckCallbackInMainThread
  // callback to determine whether the call should execute or not.
  if (!disable_eval_error_message_.IsNull())
    execution_context_->DisableEval(disable_eval_error_message_);
}

ContentSecurityPolicy::~ContentSecurityPolicy() = default;

void ContentSecurityPolicy::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
  visitor->Trace(policies_);
  visitor->Trace(console_messages_);
  visitor->Trace(self_source_);
}

Document* ContentSecurityPolicy::GetDocument() const {
  return DynamicTo<Document>(execution_context_.Get());
}

void ContentSecurityPolicy::CopyStateFrom(const ContentSecurityPolicy* other) {
  DCHECK(policies_.IsEmpty());
  for (const auto& policy : other->policies_)
    AddAndReportPolicyFromHeaderValue(policy->Header(), policy->HeaderType(),
                                      policy->HeaderSource());
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
  if (headers.ShouldParseWasmEval()) {
    supports_wasm_eval_ = true;
  }
  if (!headers.ContentSecurityPolicy().IsEmpty())
    AddAndReportPolicyFromHeaderValue(headers.ContentSecurityPolicy(),
                                      kContentSecurityPolicyHeaderTypeEnforce,
                                      kContentSecurityPolicyHeaderSourceHTTP);
  if (!headers.ContentSecurityPolicyReportOnly().IsEmpty())
    AddAndReportPolicyFromHeaderValue(headers.ContentSecurityPolicyReportOnly(),
                                      kContentSecurityPolicyHeaderTypeReport,
                                      kContentSecurityPolicyHeaderSourceHTTP);
}

void ContentSecurityPolicy::DidReceiveHeader(
    const String& header,
    ContentSecurityPolicyHeaderType type,
    ContentSecurityPolicyHeaderSource source) {
  AddAndReportPolicyFromHeaderValue(header, type, source);

  // This might be called after we've been bound to an execution context. For
  // example, a <meta> element might be injected after page load.
  if (execution_context_)
    ApplyPolicySideEffectsToExecutionContext();
}

bool ContentSecurityPolicy::ShouldEnforceEmbeddersPolicy(
    const ResourceResponse& response,
    const SecurityOrigin* parent_origin) {
  if (response.Url().IsEmpty() || response.Url().ProtocolIsAbout() ||
      response.Url().ProtocolIsData() || response.Url().ProtocolIs("blob") ||
      response.Url().ProtocolIs("filesystem")) {
    return true;
  }

  if (parent_origin->CanAccess(SecurityOrigin::Create(response.Url()).get()))
    return true;

  String header = response.HttpHeaderField(HTTPNames::Allow_CSP_From);
  header = header.StripWhiteSpace();
  if (header == "*")
    return true;
  if (scoped_refptr<const SecurityOrigin> child_origin =
          SecurityOrigin::CreateFromString(header)) {
    return parent_origin->CanAccess(child_origin.get());
  }

  return false;
}

void ContentSecurityPolicy::AddPolicyFromHeaderValue(
    const String& header,
    ContentSecurityPolicyHeaderType type,
    ContentSecurityPolicyHeaderSource source) {
  // If this is a report-only header inside a <meta> element, bail out.
  if (source == kContentSecurityPolicyHeaderSourceMeta &&
      type == kContentSecurityPolicyHeaderTypeReport) {
    ReportReportOnlyInMeta(header);
    return;
  }

  if (source == kContentSecurityPolicyHeaderSourceHTTP)
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

    if (!policy->AllowEval(nullptr,
                           SecurityViolationReportingPolicy::kSuppressReporting,
                           kWillNotThrowException, g_empty_string) &&
        disable_eval_error_message_.IsNull()) {
      disable_eval_error_message_ = policy->EvalDisabledErrorMessage();
    }

    policies_.push_back(policy.Release());

    // Skip the comma, and begin the next header from the current position.
    DCHECK(position == end || *position == ',');
    SkipExactly<UChar>(position, end, ',');
    begin = position;
  }
}

void ContentSecurityPolicy::ReportAccumulatedHeaders(
    LocalFrameClient* client) const {
  // Notify the embedder about headers that have accumulated before the
  // navigation got committed.  See comments in
  // addAndReportPolicyFromHeaderValue for more details and context.
  DCHECK(client);
  WebVector<WebContentSecurityPolicy> policies(policies_.size());
  for (wtf_size_t i = 0; i < policies_.size(); ++i)
    policies[i] = policies_[i]->ExposeForNavigationalChecks();
  client->DidAddContentSecurityPolicies(policies);
}

void ContentSecurityPolicy::AddAndReportPolicyFromHeaderValue(
    const String& header,
    ContentSecurityPolicyHeaderType type,
    ContentSecurityPolicyHeaderSource source) {
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
  WebVector<WebContentSecurityPolicy> policies(policies_.size() -
                                               previous_policy_count);
  for (wtf_size_t i = previous_policy_count; i < policies_.size(); ++i) {
    policies[i - previous_policy_count] =
        policies_[i]->ExposeForNavigationalChecks();
  }
  if (GetDocument() && GetDocument()->GetFrame()) {
    GetDocument()->GetFrame()->Client()->DidAddContentSecurityPolicies(
        policies);
  }
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
  self_source_ =
      new CSPSource(this, self_protocol_, origin->Host(), origin->Port(),
                    String(), CSPSource::kNoWildcard, CSPSource::kNoWildcard);
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
          ComputeDigest(algorithm_map.algorithm, utf8_source.Data(),
                        utf8_source.length(), digest);
      if (digest_success) {
        csp_hash_values->push_back(
            CSPHashValue(algorithm_map.csp_hash_algorithm, digest));
      }
    }
  }
}

// static
bool ContentSecurityPolicy::CheckScriptHashAgainstPolicy(
    Vector<CSPHashValue>& csp_hash_values,
    const Member<CSPDirectiveList>& policy,
    InlineType inline_type) {
  for (const auto& csp_hash_value : csp_hash_values) {
    if (policy->AllowScriptHash(csp_hash_value, inline_type)) {
      return true;
    }
  }
  return false;
}

// static
bool ContentSecurityPolicy::CheckStyleHashAgainstPolicy(
    Vector<CSPHashValue>& csp_hash_values,
    const Member<CSPDirectiveList>& policy,
    InlineType inline_type) {
  for (const auto& csp_hash_value : csp_hash_values) {
    if (policy->AllowStyleHash(csp_hash_value, inline_type)) {
      return true;
    }
  }
  return false;
}

bool ContentSecurityPolicy::AllowJavaScriptURLs(
    Element* element,
    const String& source,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    SecurityViolationReportingPolicy reporting_policy) const {
  // Javascript URLs may be whitelisted by hash, if
  // 'unsafe-hashes' is present in a policy. Check against the digest
  // of the |source| and also check whether inline script is allowed.
  Vector<CSPHashValue> csp_hash_values;
  FillInCSPHashValues(source, script_hash_algorithms_used_, &csp_hash_values);

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    is_allowed &= CheckScriptHashAgainstPolicy(csp_hash_values, policy,
                                               InlineType::kAttribute) ||
                  policy->AllowJavaScriptURLs(element, source, context_url,
                                              context_line, reporting_policy);
  }
  return is_allowed;
}

bool ContentSecurityPolicy::AllowInlineEventHandler(
    Element* element,
    const String& source,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    SecurityViolationReportingPolicy reporting_policy) const {
  // Inline event handlers may be whitelisted by hash, if
  // 'unsafe-hashes' is present in a policy. Check against the digest
  // of the |source| and also check whether inline script is allowed.
  Vector<CSPHashValue> csp_hash_values;
  FillInCSPHashValues(source, script_hash_algorithms_used_, &csp_hash_values);

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    is_allowed &=
        CheckScriptHashAgainstPolicy(csp_hash_values, policy,
                                     InlineType::kAttribute) ||
        policy->AllowInlineEventHandlers(element, source, context_url,
                                         context_line, reporting_policy);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowInlineScript(
    Element* element,
    const String& context_url,
    const String& nonce,
    const WTF::OrdinalNumber& context_line,
    const String& script_content,
    InlineType inline_type,
    SecurityViolationReportingPolicy reporting_policy) const {
  DCHECK(element);

  Vector<CSPHashValue> csp_hash_values;
  FillInCSPHashValues(script_content, script_hash_algorithms_used_,
                      &csp_hash_values);

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    is_allowed &=
        CheckScriptHashAgainstPolicy(csp_hash_values, policy, inline_type) ||
        policy->AllowInlineScript(element, context_url, nonce, context_line,
                                  reporting_policy, script_content);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowInlineStyle(
    Element* element,
    const String& context_url,
    const String& nonce,
    const WTF::OrdinalNumber& context_line,
    const String& style_content,
    InlineType inline_type,
    SecurityViolationReportingPolicy reporting_policy) const {
  DCHECK(element);

  if (override_inline_style_allowed_)
    return true;

  Vector<CSPHashValue> csp_hash_values;
  FillInCSPHashValues(style_content, style_hash_algorithms_used_,
                      &csp_hash_values);

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    is_allowed &=
        CheckStyleHashAgainstPolicy(csp_hash_values, policy, inline_type) ||
        policy->AllowInlineStyle(element, context_url, nonce, context_line,
                                 reporting_policy, style_content, inline_type);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowEval(
    ScriptState* script_state,
    SecurityViolationReportingPolicy reporting_policy,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& script_content) const {
  bool is_allowed = true;
  for (const auto& policy : policies_) {
    is_allowed &= policy->AllowEval(script_state, reporting_policy,
                                    exception_status, script_content);
  }
  return is_allowed;
}

bool ContentSecurityPolicy::AllowWasmEval(
    ScriptState* script_state,
    SecurityViolationReportingPolicy reporting_policy,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& script_content) const {
  bool is_allowed = true;
  for (const auto& policy : policies_) {
    is_allowed &= policy->AllowWasmEval(script_state, reporting_policy,
                                        exception_status, script_content);
  }
  return is_allowed;
}

String ContentSecurityPolicy::EvalDisabledErrorMessage() const {
  for (const auto& policy : policies_) {
    if (!policy->AllowEval(nullptr,
                           SecurityViolationReportingPolicy::kSuppressReporting,
                           kWillNotThrowException, g_empty_string)) {
      return policy->EvalDisabledErrorMessage();
    }
  }
  return String();
}

bool ContentSecurityPolicy::AllowPluginType(
    const String& type,
    const String& type_attribute,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) const {
  for (const auto& policy : policies_) {
    if (!policy->AllowPluginType(type, type_attribute, url, reporting_policy))
      return false;
  }
  return true;
}

bool ContentSecurityPolicy::AllowPluginTypeForDocument(
    const Document& document,
    const String& type,
    const String& type_attribute,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) const {
  if (document.GetContentSecurityPolicy() &&
      !document.GetContentSecurityPolicy()->AllowPluginType(
          type, type_attribute, url, reporting_policy))
    return false;

  // CSP says that a plugin document in a nested browsing context should
  // inherit the plugin-types of its parent.
  //
  // FIXME: The plugin-types directive should be pushed down into the
  // current document instead of reaching up to the parent for it here.
  LocalFrame* frame = document.GetFrame();
  if (frame && frame->Tree().Parent() && document.IsPluginDocument()) {
    ContentSecurityPolicy* parent_csp = frame->Tree()
                                            .Parent()
                                            ->GetSecurityContext()
                                            ->GetContentSecurityPolicy();
    if (parent_csp && !parent_csp->AllowPluginType(type, type_attribute, url,
                                                   reporting_policy))
      return false;
  }

  return true;
}

bool ContentSecurityPolicy::AllowScriptFromSource(
    const KURL& url,
    const String& nonce,
    const IntegrityMetadataSet& hashes,
    ParserDisposition parser_disposition,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  if (ShouldBypassContentSecurityPolicy(url, execution_context_)) {
    UseCounter::Count(
        GetDocument(),
        parser_disposition == kParserInserted
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
        // The schemes where javascript:-URLs are blocked are usually privileged
        // pages, so do not allow the CSP to be bypassed either.
        !SchemeRegistry::ShouldTreatURLSchemeAsNotAllowingJavascriptURLs(
            execution_context_->GetSecurityOrigin()->Protocol())) {
      return true;
    }
  }

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
      continue;
    is_allowed &=
        policy->AllowScriptFromSource(url, nonce, hashes, parser_disposition,
                                      redirect_status, reporting_policy);
  }
  return is_allowed;
}

bool ContentSecurityPolicy::AllowRequestWithoutIntegrity(
    mojom::RequestContextType context,
    const KURL& url,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  for (const auto& policy : policies_) {
    if (CheckHeaderTypeMatches(check_header_type, policy->HeaderType()) &&
        !policy->AllowRequestWithoutIntegrity(context, url, redirect_status,
                                              reporting_policy))
      return false;
  }
  return true;
}

bool ContentSecurityPolicy::AllowRequest(
    mojom::RequestContextType context,
    const KURL& url,
    const String& nonce,
    const IntegrityMetadataSet& integrity_metadata,
    ParserDisposition parser_disposition,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  if (integrity_metadata.IsEmpty() &&
      !AllowRequestWithoutIntegrity(context, url, redirect_status,
                                    reporting_policy, check_header_type)) {
    return false;
  }

  switch (context) {
    case mojom::RequestContextType::AUDIO:
    case mojom::RequestContextType::TRACK:
    case mojom::RequestContextType::VIDEO:
      return AllowMediaFromSource(url, redirect_status, reporting_policy,
                                  check_header_type);
    case mojom::RequestContextType::BEACON:
    case mojom::RequestContextType::EVENT_SOURCE:
    case mojom::RequestContextType::FETCH:
    case mojom::RequestContextType::PING:
    case mojom::RequestContextType::XML_HTTP_REQUEST:
    case mojom::RequestContextType::SUBRESOURCE:
      return AllowConnectToSource(url, redirect_status, reporting_policy,
                                  check_header_type);
    case mojom::RequestContextType::EMBED:
    case mojom::RequestContextType::OBJECT:
      return AllowObjectFromSource(url, redirect_status, reporting_policy,
                                   check_header_type);
    case mojom::RequestContextType::PREFETCH:
      return AllowPrefetchFromSource(url, redirect_status, reporting_policy,
                                     check_header_type);
    case mojom::RequestContextType::FAVICON:
    case mojom::RequestContextType::IMAGE:
    case mojom::RequestContextType::IMAGE_SET:
      return AllowImageFromSource(url, redirect_status, reporting_policy,
                                  check_header_type);
    case mojom::RequestContextType::FONT:
      return AllowFontFromSource(url, redirect_status, reporting_policy,
                                 check_header_type);
    case mojom::RequestContextType::FORM:
      return AllowFormAction(url, redirect_status, reporting_policy,
                             check_header_type);
    case mojom::RequestContextType::FRAME:
    case mojom::RequestContextType::IFRAME:
      return AllowFrameFromSource(url, redirect_status, reporting_policy,
                                  check_header_type);
    case mojom::RequestContextType::IMPORT:
    case mojom::RequestContextType::SCRIPT:
    case mojom::RequestContextType::XSLT:
      return AllowScriptFromSource(url, nonce, integrity_metadata,
                                   parser_disposition, redirect_status,
                                   reporting_policy, check_header_type);
    case mojom::RequestContextType::MANIFEST:
      return AllowManifestFromSource(url, redirect_status, reporting_policy,
                                     check_header_type);
    case mojom::RequestContextType::SERVICE_WORKER:
    case mojom::RequestContextType::SHARED_WORKER:
    case mojom::RequestContextType::WORKER:
      return AllowWorkerContextFromSource(url, redirect_status,
                                          reporting_policy, check_header_type);
    case mojom::RequestContextType::STYLE:
      return AllowStyleFromSource(url, nonce, redirect_status, reporting_policy,
                                  check_header_type);
    case mojom::RequestContextType::CSP_REPORT:
    case mojom::RequestContextType::DOWNLOAD:
    case mojom::RequestContextType::HYPERLINK:
    case mojom::RequestContextType::INTERNAL:
    case mojom::RequestContextType::LOCATION:
    case mojom::RequestContextType::PLUGIN:
    case mojom::RequestContextType::UNSPECIFIED:
      return true;
  }
  NOTREACHED();
  return true;
}

void ContentSecurityPolicy::UsesScriptHashAlgorithms(uint8_t algorithms) {
  script_hash_algorithms_used_ |= algorithms;
}

void ContentSecurityPolicy::UsesStyleHashAlgorithms(uint8_t algorithms) {
  style_hash_algorithms_used_ |= algorithms;
}

bool ContentSecurityPolicy::AllowObjectFromSource(
    const KURL& url,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  if (ShouldBypassContentSecurityPolicy(url, execution_context_))
    return true;

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
      continue;
    is_allowed &=
        policy->AllowObjectFromSource(url, redirect_status, reporting_policy);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowPrefetchFromSource(
    const KURL& url,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  if (ShouldBypassContentSecurityPolicy(url, execution_context_))
    return true;

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
      continue;
    is_allowed &=
        policy->AllowPrefetchFromSource(url, redirect_status, reporting_policy);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowFrameFromSource(
    const KURL& url,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  if (ShouldBypassContentSecurityPolicy(url, execution_context_))
    return true;

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
      continue;
    is_allowed &=
        policy->AllowFrameFromSource(url, redirect_status, reporting_policy);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowImageFromSource(
    const KURL& url,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  if (ShouldBypassContentSecurityPolicy(url, execution_context_,
                                        SchemeRegistry::kPolicyAreaImage))
    return true;

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
      continue;
    is_allowed &=
        policy->AllowImageFromSource(url, redirect_status, reporting_policy);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowStyleFromSource(
    const KURL& url,
    const String& nonce,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  if (ShouldBypassContentSecurityPolicy(url, execution_context_,
                                        SchemeRegistry::kPolicyAreaStyle))
    return true;

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
      continue;
    is_allowed &= policy->AllowStyleFromSource(url, nonce, redirect_status,
                                               reporting_policy);
  }
  return is_allowed;
}

bool ContentSecurityPolicy::AllowFontFromSource(
    const KURL& url,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  if (ShouldBypassContentSecurityPolicy(url, execution_context_))
    return true;

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
      continue;
    is_allowed &=
        policy->AllowFontFromSource(url, redirect_status, reporting_policy);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowMediaFromSource(
    const KURL& url,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  if (ShouldBypassContentSecurityPolicy(url, execution_context_))
    return true;

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
      continue;
    is_allowed &=
        policy->AllowMediaFromSource(url, redirect_status, reporting_policy);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowConnectToSource(
    const KURL& url,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  if (ShouldBypassContentSecurityPolicy(url, execution_context_))
    return true;

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
      continue;
    is_allowed &=
        policy->AllowConnectToSource(url, redirect_status, reporting_policy);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowFormAction(
    const KURL& url,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  if (ShouldBypassContentSecurityPolicy(url, execution_context_))
    return true;

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
      continue;
    is_allowed &=
        policy->AllowFormAction(url, redirect_status, reporting_policy);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowBaseURI(
    const KURL& url,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  // `base-uri` isn't affected by 'upgrade-insecure-requests', so we'll check
  // both report-only and enforce headers here.
  if (ShouldBypassContentSecurityPolicy(url, execution_context_))
    return true;

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(CheckHeaderType::kCheckAll,
                                policy->HeaderType()))
      continue;
    is_allowed &= policy->AllowBaseURI(url, redirect_status, reporting_policy);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowTrustedTypePolicy(
    const String& policy_name) const {
  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(CheckHeaderType::kCheckAll,
                                policy->HeaderType())) {
      continue;
    }
    is_allowed &= policy->AllowTrustedTypePolicy(policy_name);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowWorkerContextFromSource(
    const KURL& url,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  // CSP 1.1 moves workers from 'script-src' to the new 'child-src'. Measure the
  // impact of this backwards-incompatible change.
  // TODO(mkwst): We reverted this.
  if (Document* document = this->GetDocument()) {
    UseCounter::Count(*document, WebFeature::kWorkerSubjectToCSP);
    bool is_allowed_worker = true;
    if (!ShouldBypassContentSecurityPolicy(url, execution_context_)) {
      for (const auto& policy : policies_) {
        if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
          continue;
        is_allowed_worker &= policy->AllowWorkerFromSource(
            url, redirect_status,
            SecurityViolationReportingPolicy::kSuppressReporting);
      }
    }

    bool is_allowed_script = true;

    if (!ShouldBypassContentSecurityPolicy(url, execution_context_)) {
      for (const auto& policy : policies_) {
        if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
          continue;
        is_allowed_script &= policy->AllowScriptFromSource(
            url, AtomicString(), IntegrityMetadataSet(), kNotParserInserted,
            redirect_status,
            SecurityViolationReportingPolicy::kSuppressReporting);
      }
    }

    if (is_allowed_worker && !is_allowed_script) {
      UseCounter::Count(*document,
                        WebFeature::kWorkerAllowedByChildBlockedByScript);
    }
  }

  if (ShouldBypassContentSecurityPolicy(url, execution_context_))
    return true;

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
      continue;
    is_allowed &=
        policy->AllowWorkerFromSource(url, redirect_status, reporting_policy);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowManifestFromSource(
    const KURL& url,
    RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy,
    CheckHeaderType check_header_type) const {
  if (ShouldBypassContentSecurityPolicy(url, execution_context_))
    return true;

  bool is_allowed = true;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, policy->HeaderType()))
      continue;
    is_allowed &=
        policy->AllowManifestFromSource(url, redirect_status, reporting_policy);
  }

  return is_allowed;
}

bool ContentSecurityPolicy::AllowAncestors(
    LocalFrame* frame,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) const {
  bool is_allowed = true;
  for (const auto& policy : policies_)
    is_allowed &= policy->AllowAncestors(frame, url, reporting_policy);
  return is_allowed;
}

bool ContentSecurityPolicy::IsFrameAncestorsEnforced() const {
  for (const auto& policy : policies_) {
    if (policy->IsFrameAncestorsEnforced())
      return true;
  }
  return false;
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

const KURL ContentSecurityPolicy::Url() const {
  return execution_context_->Url();
}

void ContentSecurityPolicy::EnforceSandboxFlags(SandboxFlags mask) {
  sandbox_mask_ |= mask;
}

void ContentSecurityPolicy::TreatAsPublicAddress() {
  if (!RuntimeEnabledFeatures::CorsRFC1918Enabled())
    return;
  treat_as_public_address_ = true;
}

void ContentSecurityPolicy::RequireTrustedTypes() {
  if (!RuntimeEnabledFeatures::TrustedDOMTypesEnabled())
    return;
  require_safe_types_ = true;
}

void ContentSecurityPolicy::EnforceStrictMixedContentChecking() {
  insecure_request_policy_ |= kBlockAllMixedContent;
}

void ContentSecurityPolicy::UpgradeInsecureRequests() {
  insecure_request_policy_ |= kUpgradeInsecureRequests;
}

static String StripURLForUseInReport(
    ExecutionContext* context,
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
      context->GetSecurityOrigin()->CanRequest(url) ||
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
    ExecutionContext* context,
    const String& directive_text,
    const ContentSecurityPolicy::DirectiveType& effective_type,
    const KURL& blocked_url,
    const String& header,
    RedirectStatus redirect_status,
    ContentSecurityPolicyHeaderType header_type,
    ContentSecurityPolicy::ViolationType violation_type,
    std::unique_ptr<SourceLocation> source_location,
    const String& script_source) {
  if (effective_type == ContentSecurityPolicy::DirectiveType::kFrameAncestors) {
    // If this load was blocked via 'frame-ancestors', then the URL of
    // |document| has not yet been initialized. In this case, we'll set both
    // 'documentURI' and 'blockedURI' to the blocked document's URL.
    String stripped_url = StripURLForUseInReport(
        context, blocked_url, RedirectStatus::kNoRedirect,
        ContentSecurityPolicy::DirectiveType::kDefaultSrc);
    init->setDocumentURI(stripped_url);
    init->setBlockedURI(stripped_url);
  } else {
    String stripped_url = StripURLForUseInReport(
        context, context->Url(), RedirectStatus::kNoRedirect,
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
        init->setBlockedURI(StripURLForUseInReport(
            context, blocked_url, redirect_status, effective_type));
        break;
    }
  }

  String effective_directive =
      ContentSecurityPolicy::GetDirectiveName(effective_type);
  init->setViolatedDirective(effective_directive);
  init->setEffectiveDirective(effective_directive);
  init->setOriginalPolicy(header);
  init->setDisposition(header_type == kContentSecurityPolicyHeaderTypeEnforce
                           ? "enforce"
                           : "report");
  init->setStatusCode(0);

  // TODO(mkwst): We only have referrer and status code information for
  // Documents. It would be nice to get them for Workers as well.
  if (auto* document = DynamicTo<Document>(*context)) {
    init->setReferrer(document->referrer());
    if (!SecurityOrigin::IsSecure(context->Url()) && document->Loader())
      init->setStatusCode(document->Loader()->GetResponse().HttpStatusCode());
  }

  // If no source location is provided, use the source location of the context.
  if (!source_location)
    source_location = SourceLocation::Capture(context);
  if (source_location->LineNumber()) {
    KURL source = KURL(source_location->Url());
    init->setSourceFile(StripURLForUseInReport(context, source, redirect_status,
                                               effective_type));
    init->setLineNumber(source_location->LineNumber());
    init->setColumnNumber(source_location->ColumnNumber());
  } else {
    init->setSourceFile(String());
    init->setLineNumber(0);
    init->setColumnNumber(0);
  }

  if (!script_source.IsEmpty()) {
    init->setSample(script_source.StripWhiteSpace().Left(
        ContentSecurityPolicy::kMaxSampleLength));
  }
}

void ContentSecurityPolicy::ReportViolation(
    const String& directive_text,
    const DirectiveType& effective_type,
    const String& console_message,
    const KURL& blocked_url,
    const Vector<String>& report_endpoints,
    bool use_reporting_api,
    const String& header,
    ContentSecurityPolicyHeaderType header_type,
    ViolationType violation_type,
    std::unique_ptr<SourceLocation> source_location,
    LocalFrame* context_frame,
    RedirectStatus redirect_status,
    Element* element,
    const String& source) {
  DCHECK(violation_type == kURLViolation || blocked_url.IsEmpty());

  // TODO(lukasza): Support sending reports from OOPIFs -
  // https://crbug.com/611232 (or move CSP child-src and frame-src checks to the
  // browser process - see https://crbug.com/376522).
  if (!execution_context_ && !context_frame) {
    DCHECK(effective_type == DirectiveType::kChildSrc ||
           effective_type == DirectiveType::kFrameSrc ||
           effective_type == DirectiveType::kPluginTypes);
    return;
  }

  DCHECK((execution_context_ && !context_frame) ||
         ((effective_type == DirectiveType::kFrameAncestors) && context_frame));

  SecurityPolicyViolationEventInit violation_data;

  // If we're processing 'frame-ancestors', use |contextFrame|'s execution
  // context to gather data. Otherwise, use the policy's execution context.
  ExecutionContext* relevant_context =
      context_frame ? context_frame->GetDocument() : execution_context_;
  DCHECK(relevant_context);
  GatherSecurityPolicyViolationEventData(
      &violation_data, relevant_context, directive_text, effective_type,
      blocked_url, header, redirect_status, header_type, violation_type,
      std::move(source_location), source);

  // TODO(mkwst): Obviously, we shouldn't hit this check, as extension-loaded
  // resources should be allowed regardless. We apparently do, however, so
  // we should at least stop spamming reporting endpoints. See
  // https://crbug.com/524356 for detail.
  if (!violation_data.sourceFile().IsEmpty() &&
      ShouldBypassContentSecurityPolicy(KURL(violation_data.sourceFile()),
                                        execution_context_)) {
    return;
  }

  PostViolationReport(violation_data, context_frame, report_endpoints,
                      use_reporting_api);

  // Fire a violation event if we're working within an execution context (e.g.
  // we're not processing 'frame-ancestors').
  if (execution_context_) {
    execution_context_->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&ContentSecurityPolicy::DispatchViolationEvents,
                             WrapPersistent(this), violation_data,
                             WrapPersistent(element)));
  }
}

void ContentSecurityPolicy::PostViolationReport(
    const SecurityPolicyViolationEventInit& violation_data,
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

  std::unique_ptr<JSONObject> csp_report = JSONObject::Create();
  csp_report->SetString("document-uri", violation_data.documentURI());
  csp_report->SetString("referrer", violation_data.referrer());
  csp_report->SetString("violated-directive",
                        violation_data.violatedDirective());
  csp_report->SetString("effective-directive",
                        violation_data.effectiveDirective());
  csp_report->SetString("original-policy", violation_data.originalPolicy());
  csp_report->SetString("disposition", violation_data.disposition());
  csp_report->SetString("blocked-uri", violation_data.blockedURI());
  if (violation_data.lineNumber())
    csp_report->SetInteger("line-number", violation_data.lineNumber());
  if (violation_data.columnNumber())
    csp_report->SetInteger("column-number", violation_data.columnNumber());
  if (!violation_data.sourceFile().IsEmpty())
    csp_report->SetString("source-file", violation_data.sourceFile());
  csp_report->SetInteger("status-code", violation_data.statusCode());

  csp_report->SetString("script-sample", violation_data.sample());

  std::unique_ptr<JSONObject> report_object = JSONObject::Create();
  report_object->SetObject("csp-report", std::move(csp_report));
  String stringified_report = report_object->ToJSONString();

  // Only POST unique reports to the external endpoint; repeated reports add no
  // value on the server side, as they're indistinguishable. Note that we'll
  // fire the DOM event for every violation, as the page has enough context to
  // react in some reasonable way to each violation as it occurs.
  if (ShouldSendViolationReport(stringified_report)) {
    DidSendViolationReport(stringified_report);

    // TODO(mkwst): Support POSTing violation reports from a Worker.
    Document* document =
        context_frame ? context_frame->GetDocument() : this->GetDocument();
    if (!document)
      return;

    LocalFrame* frame = document->GetFrame();
    if (!frame)
      return;

    scoped_refptr<EncodedFormData> report =
        EncodedFormData::Create(stringified_report.Utf8());

    DEFINE_STATIC_LOCAL(ReportingServiceProxyPtrHolder,
                        reporting_service_proxy_holder, ());

    for (const auto& report_endpoint : report_endpoints) {
      if (use_reporting_api) {
        reporting_service_proxy_holder.QueueCspViolationReport(
            document->Url(), report_endpoint, violation_data);
      } else {
        // If we have a context frame we're dealing with 'frame-ancestors' and
        // we don't have our own execution context. Use the frame's document to
        // complete the endpoint URL, overriding its URL with the blocked
        // document's URL.
        DCHECK(!context_frame || !execution_context_);
        DCHECK(!context_frame ||
               GetDirectiveType(violation_data.effectiveDirective()) ==
                   DirectiveType::kFrameAncestors);
        KURL url =
            context_frame
                ? frame->GetDocument()->CompleteURLWithOverride(
                      report_endpoint, KURL(violation_data.blockedURI()))
                // We use the FallbackBaseURL to ensure that we don't
                // respect base elements when determining the report
                // endpoint URL.
                : frame->GetDocument()->CompleteURLWithOverride(
                      report_endpoint, frame->GetDocument()->FallbackBaseURL());
        PingLoader::SendViolationReport(
            frame, url, report,
            PingLoader::kContentSecurityPolicyViolationReport);
      }
    }
  }
}

void ContentSecurityPolicy::DispatchViolationEvents(
    const SecurityPolicyViolationEventInit& violation_data,
    Element* element) {
  // Worklets don't support Events in general.
  if (execution_context_->IsWorkletGlobalScope())
    return;

  SecurityPolicyViolationEvent& event = *SecurityPolicyViolationEvent::Create(
      EventTypeNames::securitypolicyviolation, violation_data);
  DCHECK(event.bubbles());

  if (auto* document = DynamicTo<Document>(*execution_context_)) {
    if (element && element->isConnected() && element->GetDocument() == document)
      element->EnqueueEvent(event, TaskType::kInternalDefault);
    else
      document->EnqueueEvent(event, TaskType::kInternalDefault);
  } else if (auto* scope = DynamicTo<WorkerGlobalScope>(*execution_context_)) {
    scope->EnqueueEvent(event, TaskType::kInternalDefault);
  }
}

void ContentSecurityPolicy::ReportMixedContent(
    const KURL& mixed_url,
    RedirectStatus redirect_status) const {
  for (const auto& policy : policies_)
    policy->ReportMixedContent(mixed_url, redirect_status);
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
  MessageLevel level = kErrorMessageLevel;
  if (EqualIgnoringASCIICase(name, kAllow)) {
    message = kAllowMessage;
  } else if (EqualIgnoringASCIICase(name, kOptions)) {
    message = kOptionsMessage;
  } else if (EqualIgnoringASCIICase(name, kPolicyURI)) {
    message = kPolicyURIMessage;
  } else if (GetDirectiveType(name) != DirectiveType::kUndefined) {
    message = "The Content-Security-Policy directive '" + name +
              "' is implemented behind a flag which is currently disabled.\n";
    level = kInfoMessageLevel;
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

void ContentSecurityPolicy::ReportInvalidRequireSRIForTokens(
    const String& invalid_tokens) {
  LogToConsole(
      "Error while parsing the 'require-sri-for' Content Security Policy "
      "directive: " +
      invalid_tokens);
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

void ContentSecurityPolicy::ReportMissingReportURI(const String& policy) {
  LogToConsole("The Content Security Policy '" + policy +
               "' was delivered in report-only mode, but does not specify a "
               "'report-uri'; the policy will have no effect. Please either "
               "add a 'report-uri' directive, or deliver the policy via the "
               "'Content-Security-Policy' header.");
}

void ContentSecurityPolicy::LogToConsole(const String& message,
                                         MessageLevel level) {
  LogToConsole(ConsoleMessage::Create(kSecurityMessageSource, level, message));
}

void ContentSecurityPolicy::LogToConsole(ConsoleMessage* console_message,
                                         LocalFrame* frame) {
  if (frame)
    frame->GetDocument()->AddConsoleMessage(console_message);
  else if (execution_context_)
    execution_context_->AddConsoleMessage(console_message);
  else
    console_messages_.push_back(console_message);
}

void ContentSecurityPolicy::ReportBlockedScriptExecutionToInspector(
    const String& directive_text) const {
  probe::scriptExecutionBlockedByCSP(execution_context_, directive_text);
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

bool ContentSecurityPolicy::ShouldBypassMainWorld(
    const ExecutionContext* context) {
  if (const auto* document = DynamicTo<Document>(context)) {
    if (document->GetFrame()) {
      return document->GetFrame()
          ->GetScriptController()
          .ShouldBypassMainWorldCSP();
    }
  }
  return false;
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
    case DirectiveType::kFrameAncestors:
      return "frame-ancestors";
    case DirectiveType::kFrameSrc:
      return "frame-src";
    case DirectiveType::kFontSrc:
      return "font-src";
    case DirectiveType::kFormAction:
      return "form-action";
    case DirectiveType::kImgSrc:
      return "img-src";
    case DirectiveType::kManifestSrc:
      return "manifest-src";
    case DirectiveType::kMediaSrc:
      return "media-src";
    case DirectiveType::kObjectSrc:
      return "object-src";
    case DirectiveType::kPrefetchSrc:
      return "prefetch-src";
    case DirectiveType::kPluginTypes:
      return "plugin-types";
    case DirectiveType::kReportURI:
      return "report-uri";
    case DirectiveType::kRequireSRIFor:
      return "require-sri-for";
    case DirectiveType::kTrustedTypes:
      return "trusted-types";
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
    case DirectiveType::kUpgradeInsecureRequests:
      return "upgrade-insecure-requests";
    case DirectiveType::kWorkerSrc:
      return "worker-src";
    case DirectiveType::kReportTo:
      return "report-to";
    case DirectiveType::kNavigateTo:
      return "navigate-to";
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
  if (name == "frame-ancestors")
    return DirectiveType::kFrameAncestors;
  if (name == "frame-src")
    return DirectiveType::kFrameSrc;
  if (name == "font-src")
    return DirectiveType::kFontSrc;
  if (name == "form-action")
    return DirectiveType::kFormAction;
  if (name == "img-src")
    return DirectiveType::kImgSrc;
  if (name == "manifest-src")
    return DirectiveType::kManifestSrc;
  if (name == "media-src")
    return DirectiveType::kMediaSrc;
  if (name == "object-src")
    return DirectiveType::kObjectSrc;
  if (name == "plugin-types")
    return DirectiveType::kPluginTypes;
  if (name == "prefetch-src")
    return DirectiveType::kPrefetchSrc;
  if (name == "report-uri")
    return DirectiveType::kReportURI;
  if (name == "require-sri-for")
    return DirectiveType::kRequireSRIFor;
  if (name == "trusted-types")
    return DirectiveType::kTrustedTypes;
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
  if (name == "upgrade-insecure-requests")
    return DirectiveType::kUpgradeInsecureRequests;
  if (name == "worker-src")
    return DirectiveType::kWorkerSrc;
  if (name == "report-to")
    return DirectiveType::kReportTo;
  if (name == "navigate-to")
    return DirectiveType::kNavigateTo;

  return DirectiveType::kUndefined;
}

bool ContentSecurityPolicy::Subsumes(const ContentSecurityPolicy& other) const {
  if (!policies_.size() || !other.policies_.size())
    return !policies_.size();
  // Required-CSP must specify only one policy.
  if (policies_.size() != 1)
    return false;

  CSPDirectiveListVector other_vector;
  for (const auto& policy : other.policies_) {
    if (!policy->IsReportOnly())
      other_vector.push_back(policy);
  }

  return policies_[0]->Subsumes(other_vector);
}

// static
bool ContentSecurityPolicy::ShouldBypassContentSecurityPolicy(
    const KURL& url,
    ExecutionContext* execution_context,
    SchemeRegistry::PolicyAreas area) {
  bool should_bypass_csp;
  if (SecurityOrigin::ShouldUseInnerURL(url)) {
    should_bypass_csp = SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(
        SecurityOrigin::ExtractInnerURL(url).Protocol(), area);
    if (should_bypass_csp) {
      UseCounter::Count(execution_context, WebFeature::kInnerSchemeBypassesCSP);
    }
  } else {
    should_bypass_csp = SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(
        url.Protocol(), area);
  }
  if (should_bypass_csp) {
    UseCounter::Count(execution_context, WebFeature::kSchemeBypassesCSP);
  }

  return should_bypass_csp;
}

// static
bool ContentSecurityPolicy::IsValidCSPAttr(const String& attr,
                                           const String& context_required_csp) {
  // we don't allow any newline characters in the CSP attributes
  if (attr.Contains('\n') || attr.Contains('\r'))
    return false;

  ContentSecurityPolicy* attr_policy = ContentSecurityPolicy::Create();
  attr_policy->AddPolicyFromHeaderValue(attr,
                                        kContentSecurityPolicyHeaderTypeEnforce,
                                        kContentSecurityPolicyHeaderSourceHTTP);
  if (!attr_policy->console_messages_.IsEmpty() ||
      attr_policy->policies_.size() != 1) {
    return false;
  }

  // Don't allow any report endpoints in "csp" attributes.
  for (auto& directiveList : attr_policy->policies_) {
    if (directiveList->ReportEndpoints().size() != 0)
      return false;
  }

  if (context_required_csp.IsEmpty() || context_required_csp.IsNull()) {
    return true;
  }

  ContentSecurityPolicy* context_policy = ContentSecurityPolicy::Create();
  context_policy->AddPolicyFromHeaderValue(
      context_required_csp, kContentSecurityPolicyHeaderTypeEnforce,
      kContentSecurityPolicyHeaderSourceHTTP);

  DCHECK(context_policy->console_messages_.IsEmpty() &&
         context_policy->policies_.size() == 1);

  return context_policy->Subsumes(*attr_policy);
}

WebContentSecurityPolicyList
ContentSecurityPolicy::ExposeForNavigationalChecks() const {
  std::vector<WebContentSecurityPolicy> policies;
  for (const auto& policy : policies_) {
    policies.push_back(policy->ExposeForNavigationalChecks());
  }

  WebContentSecurityPolicyList list;
  list.policies = policies;

  if (self_source_)
    list.self_source = self_source_->ExposeForNavigationalChecks();

  return list;
}

bool ContentSecurityPolicy::HasPolicyFromSource(
    ContentSecurityPolicyHeaderSource source) const {
  for (const auto& policy : policies_) {
    if (policy->HeaderSource() == source)
      return true;
  }
  return false;
}

}  // namespace blink
