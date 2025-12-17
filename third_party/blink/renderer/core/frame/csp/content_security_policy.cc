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

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "services/network/public/mojom/integrity_algorithm.mojom-blink.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/security_context/insecure_request_policy.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-shared.h"
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
#include "third_party/blink/renderer/core/frame/csp/csp_hash_report_body.h"
#include "third_party/blink/renderer/core/frame/csp/csp_source.h"
#include "third_party/blink/renderer/core/frame/csp/source_list_directive.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/integrity_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "v8/include/v8.h"

namespace blink {

using network::mojom::ContentSecurityPolicySource;
using network::mojom::ContentSecurityPolicyType;

namespace {

// Returns true if the given `header_type` should be checked given
// `check_header_type` and `reporting_disposition`.
bool CheckHeaderTypeMatches(
    ContentSecurityPolicy::CheckHeaderType check_header_type,
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicyType header_type) {
  switch (reporting_disposition) {
    case ReportingDisposition::kSuppressReporting:
      switch (check_header_type) {
        case ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly:
          return false;
        case ContentSecurityPolicy::CheckHeaderType::kCheckAll:
        case ContentSecurityPolicy::CheckHeaderType::kCheckEnforce:
          return header_type == ContentSecurityPolicyType::kEnforce;
      }
    case ReportingDisposition::kReport:
      switch (check_header_type) {
        case ContentSecurityPolicy::CheckHeaderType::kCheckAll:
          return true;
        case ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly:
          return header_type == ContentSecurityPolicyType::kReport;
        case ContentSecurityPolicy::CheckHeaderType::kCheckEnforce:
          return header_type == ContentSecurityPolicyType::kEnforce;
      }
  }
  NOTREACHED();
}

// 3. If request’s destination is "fencedframe", and this directive’s value does
//    not contain either "https:", "https://*:*", or "*", return "Blocked".
// https://wicg.github.io/fenced-frame/#csp-algorithms
bool AllowOpaqueFencedFrames(
    const network::mojom::blink::CSPSourcePtr& source) {
  if (source->scheme != url::kHttpsScheme) {
    return false;
  }

  // "https:" is allowed.
  if (source->host.empty() && !source->is_host_wildcard) {
    return true;
  }

  // "https://*:*" is allowed.
  if (source->is_host_wildcard && source->is_port_wildcard) {
    return true;
  }

  // "https://*" is not allowed as it could leak data about ports.

  return false;
}

// Returns true if the CSP for the document loading the fenced frame allows all
// HTTPS origins for "fenced-frame-src".
bool AllowOpaqueFencedFrames(
    const network::mojom::blink::ContentSecurityPolicyPtr& policy) {
  CSPOperativeDirective directive = CSPDirectiveListOperativeDirective(
      *policy, network::mojom::CSPDirectiveName::FencedFrameSrc);
  if (directive.type == network::mojom::CSPDirectiveName::Unknown) {
    return true;
  }

  // "*" is allowed.
  if (directive.source_list->allow_star) {
    return true;
  }

  for (const auto& source : directive.source_list->sources) {
    if (AllowOpaqueFencedFrames(source)) {
      return true;
    }
  }

  return false;
}

bool HasScriptUnsafeHashes(
    const network::mojom::blink::ContentSecurityPolicy& policy) {
  CSPOperativeDirective directive = CSPDirectiveListOperativeDirective(
      policy, network::mojom::CSPDirectiveName::ScriptSrc);
  return directive.source_list && directive.source_list->allow_unsafe_hashes;
}

const char kWarningMessageForSyntheticResponse[] =
    " Since the synthetic response is enabled on this page, scripts are "
    "blocked until the new Content Security Policy is added via the <meta> "
    "tag.";
}  // namespace

// https://www.w3.org/TR/CSP3/#strip-url-for-use-in-reports
String ContentSecurityPolicy::StripURLForUseInReport(
    const SecurityOrigin* security_origin,
    const KURL& url,
    CSPDirectiveName effective_type) {
  if (!url.IsValid()) {
    return String();
  }

  // https://www.w3.org/TR/CSP3/#strip-url-for-use-in-reports
  // > 1. If url's scheme is not "`https`", "'http'", "`wss`" or "`ws`" then
  // >    return url's scheme.
  static const char* const allow_list[] = {"http", "https", "ws", "wss"};
  if (!base::Contains(allow_list, url.Protocol())) {
    return url.Protocol();
  }

  // Until we're more careful about the way we deal with navigations in frames
  // (and, by extension, in plugin documents), strip cross-origin 'frame-src'
  // and 'object-src' violations down to an origin. https://crbug.com/633306
  bool can_safely_expose_url =
      security_origin->CanRequest(url) ||
      (effective_type != CSPDirectiveName::FrameSrc &&
       effective_type != CSPDirectiveName::ObjectSrc &&
       effective_type != CSPDirectiveName::FencedFrameSrc);

  if (!can_safely_expose_url) {
    return SecurityOrigin::Create(url)->ToString();
  }

  // https://www.w3.org/TR/CSP3/#strip-url-for-use-in-reports
  // > 5. Return the result of executing the URL serializer on url.
  return CSPStripURL(url).GetString();
}

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
      if (name.FindIgnoringASCIICase(kScriptString) != kNotFound ||
          name.FindIgnoringASCIICase(kStyleString) != kNotFound ||
          value.FindIgnoringASCIICase(kScriptString) != kNotFound ||
          value.FindIgnoringASCIICase(kStyleString) != kNotFound) {
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

static WebFeature GetUseCounterType(ContentSecurityPolicyType type) {
  switch (type) {
    case ContentSecurityPolicyType::kEnforce:
      return WebFeature::kContentSecurityPolicy;
    case ContentSecurityPolicyType::kReport:
      return WebFeature::kContentSecurityPolicyReportOnly;
  }
  NOTREACHED();
}

ContentSecurityPolicy::ContentSecurityPolicy()
    : delegate_(nullptr),
      override_inline_style_allowed_(false),
      sandbox_mask_(network::mojom::blink::WebSandboxFlags::kNone),
      require_trusted_types_(false),
      insecure_request_policy_(
          mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone) {}

bool ContentSecurityPolicy::IsBound() {
  return delegate_ != nullptr;
}

void ContentSecurityPolicy::BindToDelegate(
    ContentSecurityPolicyDelegate& delegate) {
  // TODO(crbug.com/915954): Add DCHECK(!delegate_). It seems some call sites
  // call this function multiple times.
  delegate_ = &delegate;
  ApplyPolicySideEffectsToDelegate();

  // Report use counters for all the policies that have been parsed until now.
  ReportUseCounters(policies_);
  delegate_->DidAddContentSecurityPolicies(mojo::Clone(GetParsedPolicies()));
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

  // We disable 'eval()' even in the case of report-only policies, and rely on
  // the check in the V8Initializer::codeGenerationCheckCallbackInMainThread
  // callback to determine whether the call should execute or not.
  if (!disable_eval_error_message_.IsNull())
    delegate_->DisableEval(disable_eval_error_message_);

  if (!disable_wasm_eval_error_message_.IsNull())
    delegate_->SetWasmEvalErrorMessage(disable_wasm_eval_error_message_);
}

void ContentSecurityPolicy::ReportUseCounters(
    const Vector<network::mojom::blink::ContentSecurityPolicyPtr>& policies) {
  for (const auto& policy : policies) {
    Count(GetUseCounterType(policy->header->type));
    if (CSPDirectiveListAllowDynamic(*policy,
                                     CSPDirectiveName::ScriptSrcAttr) ||
        CSPDirectiveListAllowDynamic(*policy,
                                     CSPDirectiveName::ScriptSrcElem)) {
      Count(WebFeature::kCSPWithStrictDynamic);
    }

    // This use counter is for the 'unsafe-eval' keyword. We pass an empty array
    // of hashes so this is logged if and only if 'unsafe-eval' is set,
    // regardless of hashes.
    if (CSPDirectiveListAllowEval(
            *policy, this, ReportingDisposition::kSuppressReporting,
            kWillNotThrowException, g_empty_string,
            /*script_hash_values=*/{},
            delegate_ && delegate_->ScriptSrcExtendedHashesEnabled())) {
      Count(WebFeature::kCSPWithUnsafeEval);
    }

    if (HasScriptUnsafeHashes(*policy)) {
      Count(WebFeature::kCSPWithUnsafeHashes);
    }

    // We consider a policy to be "reasonably secure" if it:
    //
    // 1.  Asserts `object-src 'none'`.
    // 2.  Asserts `base-uri 'none'` or `base-uri 'self'`.
    // 3.  Avoids URL-based matching, in favor of hashes and nonces.
    //
    // https://chromium.googlesource.com/chromium/src/+/main/docs/security/web-mitigation-metrics.md
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

    CSPOperativeDirective script_src = CSPDirectiveListOperativeDirective(
        *policy, network::mojom::CSPDirectiveName::ScriptSrc);
    if (script_src.source_list) {
      if (CSPSourceListIsEvalHashPresent(*script_src.source_list)) {
        Count(WebFeature::kCSPEvalHashes);
      }
      if (!script_src.source_list->url_hashes.empty()) {
        Count(WebFeature::kCSPUrlHashes);
      }
    }
  }
}

ContentSecurityPolicy::~ContentSecurityPolicy() = default;

void ContentSecurityPolicy::Trace(Visitor* visitor) const {
  visitor->Trace(delegate_);
  visitor->Trace(console_messages_);
}

void ContentSecurityPolicy::AddPolicies(
    Vector<network::mojom::blink::ContentSecurityPolicyPtr> policies) {
  Vector<network::mojom::blink::ContentSecurityPolicyPtr> policies_to_report;
  if (delegate_) {
    policies_to_report = mojo::Clone(policies);
  }

  for (network::mojom::blink::ContentSecurityPolicyPtr& policy : policies) {
    ComputeInternalStateForParsedPolicy(*policy);

    // Report parsing errors in the console.
    for (const String& message : policy->parsing_errors)
      LogToConsole(message);

    policies_.push_back(std::move(policy));
  }

  // Reevaluate whether the composite set of enforced policies are "strict"
  // after these new policies have been added. Since additional policies can
  // only tighten the composite policy, we only need to check this if the policy
  // isn't already "strict".
  if (!enforces_strict_policy_) {
    const bool is_object_restriction_reasonable =
        std::ranges::any_of(policies_, [](const auto& policy) {
          return !CSPDirectiveListIsReportOnly(*policy) &&
                 CSPDirectiveListIsObjectRestrictionReasonable(*policy);
        });
    const bool is_base_restriction_reasonable =
        std::ranges::any_of(policies_, [](const auto& policy) {
          return !CSPDirectiveListIsReportOnly(*policy) &&
                 CSPDirectiveListIsBaseRestrictionReasonable(*policy);
        });
    const bool is_script_restriction_reasonable =
        std::ranges::any_of(policies_, [](const auto& policy) {
          return !CSPDirectiveListIsReportOnly(*policy) &&
                 CSPDirectiveListIsScriptRestrictionReasonable(*policy);
        });
    enforces_strict_policy_ = is_object_restriction_reasonable &&
                              is_base_restriction_reasonable &&
                              is_script_restriction_reasonable;
  }

  // If this ContentSecurityPolicy is not bound to a delegate yet, return. The
  // following logic will be executed in BindToDelegate when that will happen.
  if (!delegate_)
    return;

  ApplyPolicySideEffectsToDelegate();
  ReportUseCounters(policies_to_report);

  delegate_->DidAddContentSecurityPolicies(std::move(policies_to_report));
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
  if (CSPDirectiveListShouldDisableEval(
          csp, disable_eval_message,
          delegate_ && delegate_->ScriptSrcExtendedHashesEnabled()) &&
      disable_eval_error_message_.IsNull()) {
    disable_eval_error_message_ = disable_eval_message;
  }

  String disable_wasm_eval_message;
  if (CSPDirectiveListShouldDisableWasmEval(csp, this,
                                            disable_wasm_eval_message) &&
      disable_wasm_eval_error_message_.IsNull()) {
    disable_wasm_eval_error_message_ = disable_wasm_eval_message;
  }

  for (const auto& directive : csp.directives) {
    // This might cause marginal performance overhead, insofar as it combines
    // the hashing algorithms used by script and style directives. If this is
    // a bottleneck in the future, we can split things out into script and
    // style hash algorithms independently.
    switch (directive.key) {
      // These directives control scripts and stylesheets, so their algorithms
      // are the ones that matter.
      case CSPDirectiveName::DefaultSrc:
      case CSPDirectiveName::ScriptSrc:
      case CSPDirectiveName::ScriptSrcAttr:
      case CSPDirectiveName::ScriptSrcElem:
      case CSPDirectiveName::ScriptSrcV2:
      case CSPDirectiveName::StyleSrc:
      case CSPDirectiveName::StyleSrcAttr:
      case CSPDirectiveName::StyleSrcElem:
        for (const auto& hash_source : directive.value->hashes) {
          UsesHashAlgorithm(hash_source.algorithm);
        }
        for (const auto& hash_source : directive.value->eval_hashes) {
          UsesHashAlgorithm(hash_source.algorithm);
        }
        break;
      // Images, fonts, etc. do not support integrity checks, so we can skip
      // them here.
      case CSPDirectiveName::BaseURI:
      case CSPDirectiveName::BlockAllMixedContent:
      case CSPDirectiveName::ChildSrc:
      case CSPDirectiveName::ConnectSrc:
      case CSPDirectiveName::FencedFrameSrc:
      case CSPDirectiveName::FontSrc:
      case CSPDirectiveName::FormAction:
      case CSPDirectiveName::FrameAncestors:
      case CSPDirectiveName::FrameSrc:
      case CSPDirectiveName::ImgSrc:
      case CSPDirectiveName::ManifestSrc:
      case CSPDirectiveName::MediaSrc:
      case CSPDirectiveName::ObjectSrc:
      case CSPDirectiveName::ReportTo:
      case CSPDirectiveName::ReportURI:
      case CSPDirectiveName::RequireTrustedTypesFor:
      case CSPDirectiveName::Sandbox:
      case CSPDirectiveName::TreatAsPublicAddress:
      case CSPDirectiveName::TrustedTypes:
      case CSPDirectiveName::UpgradeInsecureRequests:
      case CSPDirectiveName::WorkerSrc:
        break;

      case CSPDirectiveName::Unknown:
        NOTREACHED();
    }

    // If `disallow_script_for_synthetic_response_` is true, that means the
    // synthetic response is used, and the script policy is enforced. This
    // enforcement is completed if the new policy is added via meta tag.
    if (disallow_script_for_synthetic_response_ &&
        csp.header->source == ContentSecurityPolicySource::kMeta) {
      disallow_script_for_synthetic_response_ = false;
      base::UmaHistogramCounts100(
          kSyntheticResponseBlockedResourceCountHistogramName,
          blocked_count_for_synthetic_response_);
    }
  }
}

void ContentSecurityPolicy::SetOverrideAllowInlineStyle(bool value) {
  override_inline_style_allowed_ = value;
}

// static
bool ContentSecurityPolicy::CheckHashAgainstPolicy(
    Vector<network::IntegrityMetadata>& csp_hash_values,
    const network::mojom::blink::ContentSecurityPolicy& csp,
    InlineType inline_type) {
  for (const auto& csp_hash_value : csp_hash_values) {
    if (CSPDirectiveListAllowHash(csp, csp_hash_value, inline_type)) {
      return true;
    }
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
    const OrdinalNumber& context_line,
    ReportingDisposition reporting_disposition) {
  DCHECK(element || inline_type == InlineType::kScriptAttribute ||
         inline_type == InlineType::kNavigation);

  const bool is_script = IsScriptInlineType(inline_type);
  if (!is_script && override_inline_style_allowed_) {
    return true;
  }

  // If `disallow_script_for_synthetic_response_` is true, it always returns
  // false for scripts that are likely covered by `script-src`.
  if (disallow_script_for_synthetic_response_) {
    String message;
    switch (inline_type) {
      case ContentSecurityPolicy::InlineType::kNavigation:
        message = "run the JavaScript URL.";
        break;
      case ContentSecurityPolicy::InlineType::kScriptSpeculationRules:
        message = "apply inline speculation rules.";
        break;
      case ContentSecurityPolicy::InlineType::kScriptAttribute:
        message = "execute inline event handler.";
        break;
      case ContentSecurityPolicy::InlineType::kScript:
        message = "execute inline script.";
        break;
      default:
        break;
    }
    if (!message.empty()) {
      blocked_count_for_synthetic_response_++;
      base::UmaHistogramEnumeration(
          kSyntheticResponseBlockedInlineResourceTypeHistogramName,
          inline_type);
      LogToConsole(StrCat(
          {"Refused to ", message, kWarningMessageForSyntheticResponse}));
      return false;
    }
  }

  Vector<network::IntegrityMetadata> csp_hash_values;
  FillInCSPHashValues(content, hash_algorithms_used_, csp_hash_values);

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
    case ContentSecurityPolicy::InlineType::kScriptSpeculationRules:
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
  return TrustedTypesRequired();
}

bool ContentSecurityPolicy::AllowEval(
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& script_content) {
  bool is_allowed = true;
  Vector<network::IntegrityMetadata> csp_hash_values;
  FillInCSPHashValues(script_content, hash_algorithms_used_, csp_hash_values);
  for (const auto& policy : policies_) {
    is_allowed &= CSPDirectiveListAllowEval(
        *policy, this, reporting_disposition, exception_status, script_content,
        csp_hash_values,
        delegate_ && delegate_->ScriptSrcExtendedHashesEnabled());
  }
  return is_allowed;
}

bool ContentSecurityPolicy::AllowWasmCodeGeneration(
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& script_content) {
  bool is_allowed = true;
  for (const auto& policy : policies_) {
    is_allowed &= CSPDirectiveListAllowWasmCodeGeneration(
        *policy, this, reporting_disposition, exception_status, script_content);
  }
  return is_allowed;
}

bool ContentSecurityPolicy::AllowTrustedTypesEval(
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status) {
  // https://www.w3.org/TR/CSP3/#can-compile-strings, step 5.3.3
  // "If source-list contains a source expression which is [...] "'unsafe-eval'"

  // The spec wants 'trusted-types-eval' to apply _only_ when Trusted Types is
  // enforced. Note that in most other contexts, we only check whether TT is
  // required -- report only or not -- because even in report-only we need to
  // run the checks in order to generate the reports. This, however, requires
  // TT to enabled in an enforcing directive. Because that is a global
  // property, we need to check it here.
  bool trusted_types_is_enforced =
      std::ranges::any_of(policies_, [&](const auto& policy) {
        return CSPDirectiveListRequiresTrustedTypesEnforcing(*policy);
      });
  bool trusted_types_eval_applies =
      std::ranges::any_of(policies_, [&](const auto& policy) {
        return CSPDirectiveListAllowTrustedTypesEval(
            *policy, this, reporting_disposition, exception_status);
      });
  return trusted_types_is_enforced && trusted_types_eval_applies;
}

HashSet<HashAlgorithm> ContentSecurityPolicy::HashesToReport() const {
  HashSet<HashAlgorithm> algorithms;
  for (const auto& policy : policies_) {
    if (auto algorithm = CSPDirectiveListHashToReport(*policy)) {
      algorithms.insert(algorithm.value());
    }
  }
  return algorithms;
}

void ContentSecurityPolicy::AddHashReportIfNeeded(
    LocalFrame* frame,
    const String& url,
    const HashMap<HashAlgorithm, String>& integrity_hashes) const {
  LocalDOMWindow* window = frame->DomWindow();
  CHECK(window->document());
  for (const auto& policy : policies_) {
    if (auto algorithm = CSPDirectiveListHashToReport(*policy)) {
      auto hash_it = integrity_hashes.find(algorithm.value());
      String integrity_hash = "";
      if (hash_it != integrity_hashes.end()) {
        integrity_hash = hash_it->value;
      }

      CSPHashReportBody* body = MakeGarbageCollected<CSPHashReportBody>(
          url, integrity_hash, "subresource", "script");
      Report* report_to_queue = MakeGarbageCollected<Report>(
          ReportType::kCSPHash,
          StripURLForUseInReport(
              window->GetContentSecurityPolicyDelegate().GetSecurityOrigin(),
              window->document()->Url(), CSPDirectiveName::DefaultSrc),
          body);

      ReportingContext::From(window)->QueueReport(report_to_queue,
                                                  policy->report_endpoints);
    }
  }
}

String ContentSecurityPolicy::EvalDisabledErrorMessage() const {
  for (const auto& policy : policies_) {
    String message;
    if (CSPDirectiveListShouldDisableEval(
            *policy, message,
            delegate_ && delegate_->ScriptSrcExtendedHashesEnabled())) {
      return message;
    }
  }
  return String();
}

String ContentSecurityPolicy::WasmEvalDisabledErrorMessage() const {
  for (const auto& policy : policies_) {
    String message;
    if (CSPDirectiveListShouldDisableWasmEval(*policy, this, message))
      return message;
  }
  return String();
}

namespace {
std::optional<CSPDirectiveName> GetDirectiveTypeFromRequestContextType(
    mojom::blink::RequestContextType context) {
  switch (context) {
    case mojom::blink::RequestContextType::AUDIO:
    case mojom::blink::RequestContextType::TRACK:
    case mojom::blink::RequestContextType::VIDEO:
      return CSPDirectiveName::MediaSrc;

    case mojom::blink::RequestContextType::ATTRIBUTION_SRC:
    case mojom::blink::RequestContextType::BEACON:
    case mojom::blink::RequestContextType::EVENT_SOURCE:
    case mojom::blink::RequestContextType::FETCH:
    case mojom::blink::RequestContextType::JSON:
    case mojom::blink::RequestContextType::PING:
    case mojom::blink::RequestContextType::XML_HTTP_REQUEST:
    case mojom::blink::RequestContextType::SUBRESOURCE:
    case mojom::blink::RequestContextType::SUBRESOURCE_WEBBUNDLE:
      return CSPDirectiveName::ConnectSrc;

    case mojom::blink::RequestContextType::EMBED:
    case mojom::blink::RequestContextType::OBJECT:
      return CSPDirectiveName::ObjectSrc;

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

    case mojom::blink::RequestContextType::PREFETCH:
      return CSPDirectiveName::DefaultSrc;

    case mojom::blink::RequestContextType::CSP_REPORT:
    case mojom::blink::RequestContextType::DOWNLOAD:
    case mojom::blink::RequestContextType::HYPERLINK:
    case mojom::blink::RequestContextType::INTERNAL:
    case mojom::blink::RequestContextType::LOCATION:
    case mojom::blink::RequestContextType::PLUGIN:
    case mojom::blink::RequestContextType::SPECULATION_RULES:
    case mojom::blink::RequestContextType::UNSPECIFIED:
      return std::nullopt;
  }
}

// [spec] https://w3c.github.io/webappsec-csp/#does-resource-hint-violate-policy
bool AllowResourceHintRequestForPolicy(
    network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    const KURL& document_url,
    const KURL& url,
    const String& nonce,
    const IntegrityMetadataSet& integrity_metadata,
    ParserDisposition parser_disposition,
    const KURL& url_before_redirects,
    RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    bool script_src_extended_hashes_enabled) {
  // The loop ignores default-src directives, which is the directive to report
  // for resource hints. So we don't need to check report-only policies.
  if (csp.header->type == ContentSecurityPolicyType::kEnforce) {
    for (CSPDirectiveName type : {
             CSPDirectiveName::ChildSrc,
             CSPDirectiveName::ConnectSrc,
             CSPDirectiveName::FontSrc,
             CSPDirectiveName::FrameSrc,
             CSPDirectiveName::ImgSrc,
             CSPDirectiveName::ManifestSrc,
             CSPDirectiveName::MediaSrc,
             CSPDirectiveName::ObjectSrc,
             CSPDirectiveName::ScriptSrc,
             CSPDirectiveName::ScriptSrcV2,
             CSPDirectiveName::ScriptSrcElem,
             CSPDirectiveName::StyleSrc,
             CSPDirectiveName::StyleSrcElem,
             CSPDirectiveName::WorkerSrc,
         }) {
      if (CSPDirectiveListAllowFromSource(
              csp, policy, type, document_url, url, url_before_redirects,
              redirect_status, ReportingDisposition::kSuppressReporting,
              script_src_extended_hashes_enabled, nonce, integrity_metadata,
              parser_disposition)) {
        return true;
      }
    }
  }
  // Check default-src with the given reporting disposition, to allow reporting
  // if needed.
  return CSPDirectiveListAllowFromSource(
             csp, policy, CSPDirectiveName::DefaultSrc, document_url, url,
             url_before_redirects, redirect_status, reporting_disposition,
             script_src_extended_hashes_enabled, nonce, integrity_metadata,
             parser_disposition)
      .IsAllowed();
}

}  // namespace

// https://w3c.github.io/webappsec-csp/#does-request-violate-policy
bool ContentSecurityPolicy::AllowRequest(
    mojom::blink::RequestContextType request_context,
    network::mojom::RequestDestination request_destination,
    network::mojom::RequestMode request_mode,
    const KURL& url,
    const String& nonce,
    const IntegrityMetadataSet& integrity_metadata,
    ParserDisposition parser_disposition,
    const KURL& url_before_redirects,
    RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    CheckHeaderType check_header_type) {
  // [spec] https://w3c.github.io/webappsec-csp/#does-request-violate-policy
  // 1. If request’s initiator is "prefetch", then return the result of
  // executing "Does resource hint request violate policy?" on request and
  // policy.
  if (request_context == mojom::blink::RequestContextType::PREFETCH) {
    return std::ranges::all_of(policies_, [&](const auto& policy) {
      return !CheckHeaderTypeMatches(check_header_type, reporting_disposition,
                                     policy->header->type) ||
             AllowResourceHintRequestForPolicy(
                 *policy, this, delegate_->Url(), url, nonce,
                 integrity_metadata, parser_disposition, url_before_redirects,
                 redirect_status, reporting_disposition,
                 delegate_ && delegate_->ScriptSrcExtendedHashesEnabled());
    });
  }

  std::optional<CSPDirectiveName> type =
      GetDirectiveTypeFromRequestContextType(request_context);

  if (!type)
    return true;
  return AllowFromSource(*type, url, url_before_redirects, redirect_status,
                         reporting_disposition, check_header_type, nonce,
                         integrity_metadata, parser_disposition);
}

void ContentSecurityPolicy::UsesHashAlgorithm(IntegrityAlgorithm algorithm) {
  hash_algorithms_used_.insert(algorithm);
}

bool ContentSecurityPolicy::AllowFromSource(
    CSPDirectiveName type,
    const KURL& url,
    const KURL& url_before_redirects,
    RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    CheckHeaderType check_header_type,
    const String& nonce,
    const IntegrityMetadataSet& integrity_metadata,
    ParserDisposition parser_disposition) {
  SchemeRegistry::PolicyAreas area = SchemeRegistry::kPolicyAreaAll;
  if (type == CSPDirectiveName::ImgSrc)
    area = SchemeRegistry::kPolicyAreaImage;
  else if (type == CSPDirectiveName::StyleSrcElem)
    area = SchemeRegistry::kPolicyAreaStyle;

  if (disallow_script_for_synthetic_response_) {
    SyntheticResponseBlockedSrcType blocked_src_type =
        type == CSPDirectiveName::ScriptSrcElem
            ? SyntheticResponseBlockedSrcType::kScriptSrcElm
        : type == CSPDirectiveName::WorkerSrc
            ? SyntheticResponseBlockedSrcType::kWorkerSrc
            : SyntheticResponseBlockedSrcType::kUnspecified;
    if (blocked_src_type != SyntheticResponseBlockedSrcType::kUnspecified) {
      blocked_count_for_synthetic_response_++;
      base::UmaHistogramEnumeration(
          kSyntheticResponseBlockedSrcTypeHistogramName, blocked_src_type);
      LogToConsole(StrCat({"The script from ", url.GetString(), " was blocked.",
                           kWarningMessageForSyntheticResponse}));
    }
    return false;
  }

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

  CSPCheckResult result = CSPCheckResult::Allowed();
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(check_header_type, reporting_disposition,
                                policy->header->type)) {
      continue;
    }
    result &= CSPDirectiveListAllowFromSource(
        *policy, this, type, delegate_->Url(), url, url_before_redirects,
        redirect_status, reporting_disposition,
        delegate_ && delegate_->ScriptSrcExtendedHashesEnabled(), nonce,
        integrity_metadata, parser_disposition);
  }

  if (result.WouldBlockIfWildcardDoesNotMatchWs()) {
    Count(WebFeature::kCspWouldBlockIfWildcardDoesNotMatchWs);
  }

  return result.IsAllowed();
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
    const IntegrityMetadataSet& integrity_metadata,
    ParserDisposition parser_disposition,
    const KURL& url_before_redirects,
    RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    CheckHeaderType check_header_type) {
  return AllowFromSource(CSPDirectiveName::ScriptSrcElem, url,
                         url_before_redirects, redirect_status,
                         reporting_disposition, check_header_type, nonce,
                         integrity_metadata, parser_disposition);
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
    AllowTrustedTypePolicyDetails& violation_details,
    std::optional<base::UnguessableToken> issue_id) {
  bool is_allowed = true;
  violation_details = AllowTrustedTypePolicyDetails::kAllowed;
  for (const auto& policy : policies_) {
    if (!CheckHeaderTypeMatches(CheckHeaderType::kCheckAll,
                                ReportingDisposition::kReport,
                                policy->header->type)) {
      continue;
    }
    auto new_violation_details = AllowTrustedTypePolicyDetails::kAllowed;
    bool new_allowed = CSPDirectiveListAllowTrustedTypePolicy(
        *policy, this, policy_name, is_duplicate, new_violation_details,
        issue_id);
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
    const String& sample_prefix,
    std::optional<base::UnguessableToken> issue_id) {
  bool allow = true;
  for (const auto& policy : policies_) {
    allow &= CSPDirectiveListAllowTrustedTypeAssignmentFailure(
        *policy, this, message, sample, sample_prefix, issue_id);
  }
  return allow;
}

bool ContentSecurityPolicy::IsActive() const {
  return !policies_.empty();
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

namespace {
SourceLocation* GatherSecurityPolicyViolationEventData(
    SecurityPolicyViolationEventInit* init,
    ContentSecurityPolicyDelegate* delegate,
    const String& directive_text,
    CSPDirectiveName effective_type,
    const KURL& blocked_url,
    const String& header,
    ContentSecurityPolicyType header_type,
    ContentSecurityPolicyViolationType violation_type,
    SourceLocation* source_location,
    const String& script_source,
    const String& sample_prefix,
    const std::optional<String> eval_hash) {
  if (effective_type == CSPDirectiveName::FrameAncestors) {
    // If this load was blocked via 'frame-ancestors', then the URL of
    // |document| has not yet been initialized. In this case, we'll set both
    // 'documentURI' and 'blockedURI' to the blocked document's URL.
    String stripped_url = ContentSecurityPolicy::StripURLForUseInReport(
        delegate->GetSecurityOrigin(), blocked_url,
        CSPDirectiveName::DefaultSrc);
    init->setDocumentURI(stripped_url);
    init->setBlockedURI(stripped_url);
  } else {
    String stripped_url = ContentSecurityPolicy::StripURLForUseInReport(
        delegate->GetSecurityOrigin(), delegate->Url(),
        CSPDirectiveName::DefaultSrc);
    init->setDocumentURI(stripped_url);
    switch (violation_type) {
      case ContentSecurityPolicyViolationType::kInlineViolation:
        init->setBlockedURI("inline");
        break;
      case ContentSecurityPolicyViolationType::kEvalViolation:
        init->setBlockedURI("eval");
        if (eval_hash.has_value() && !eval_hash->IsNull()) {
          init->setEvalHash(eval_hash.value());
        }
        break;
      case ContentSecurityPolicyViolationType::kWasmEvalViolation:
        init->setBlockedURI("wasm-eval");
        break;
      case ContentSecurityPolicyViolationType::kURLViolation:
      case ContentSecurityPolicyViolationType::kSRIViolation:
        // We pass RedirectStatus::kNoRedirect so that StripURLForUseInReport
        // does not strip path and query from the URL. This is safe since
        // blocked_url at this point is always the original url (before
        // redirects).
        init->setBlockedURI(ContentSecurityPolicy::StripURLForUseInReport(
            delegate->GetSecurityOrigin(), blocked_url, effective_type));
        break;
      case ContentSecurityPolicyViolationType::kTrustedTypesSinkViolation:
        init->setBlockedURI("trusted-types-sink");
        break;
      case ContentSecurityPolicyViolationType::kTrustedTypesPolicyViolation:
        init->setBlockedURI("trusted-types-policy");
        break;
    }
  }

  String effective_directive =
      ContentSecurityPolicy::GetDirectiveName(effective_type);
  init->setViolatedDirective(effective_directive);
  init->setEffectiveDirective(effective_directive);
  init->setOriginalPolicy(header);
  init->setDisposition(
      header_type == ContentSecurityPolicyType::kEnforce
          ? V8SecurityPolicyViolationEventDisposition::Enum::kEnforce
          : V8SecurityPolicyViolationEventDisposition::Enum::kReport);
  init->setStatusCode(0);

  // See https://w3c.github.io/webappsec-csp/#create-violation-for-global.
  // Step 3. If global is a Window object, set violation’s referrer to global’s
  // document's referrer. [spec text]
  String referrer = delegate->GetDocumentReferrer();
  if (referrer)
    init->setReferrer(referrer);

  // Step 4. Set violation’s status to the HTTP status code for the resource
  // associated with violation’s global object. [spec text]
  std::optional<uint16_t> status_code = delegate->GetStatusCode();
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
    String source_file = ContentSecurityPolicy::StripURLForUseInReport(
        delegate->GetSecurityOrigin(), source_url, effective_type);

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
  if (!sample_prefix.empty()) {
    sample.Append(sample_prefix.StripWhiteSpace().Left(
        ContentSecurityPolicy::kMaxSampleLength));
    sample.Append("|");
  }
  if (!script_source.empty()) {
    sample.Append(script_source.StripWhiteSpace().Left(
        ContentSecurityPolicy::kMaxSampleLength));
  }
  if (!sample.empty())
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
    SourceLocation* source_location,
    LocalFrame* context_frame,
    Element* element,
    const String& source,
    const String& source_prefix,
    std::optional<base::UnguessableToken> issue_id,
    std::optional<String> eval_hash) {
  CHECK(violation_type == kURLViolation || blocked_url.IsEmpty() ||
        violation_type == kSRIViolation);

  // TODO(crbug.com/1279745): Remove/clarify what this block is about.
  if (!delegate_ && !context_frame) {
    DCHECK(effective_type == CSPDirectiveName::ChildSrc ||
           effective_type == CSPDirectiveName::FrameSrc ||
           effective_type == CSPDirectiveName::TrustedTypes ||
           effective_type == CSPDirectiveName::RequireTrustedTypesFor ||
           effective_type == CSPDirectiveName::FencedFrameSrc);
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
      blocked_url, header, header_type, violation_type, source_location, source,
      source_prefix, eval_hash);

  // TODO(mkwst): Obviously, we shouldn't hit this check, as extension-loaded
  // resources should be allowed regardless. We apparently do, however, so
  // we should at least stop spamming reporting endpoints. See
  // https://crbug.com/524356 for detail.
  if (!violation_data->sourceFile().empty() &&
      ShouldBypassContentSecurityPolicy(KURL(violation_data->sourceFile()))) {
    return;
  }
  PostViolationReport(violation_data, context_frame, report_endpoints,
                      use_reporting_api);

  // Fire a violation event if we're working with a delegate and we don't have a
  // `context_frame` (i.e. we're not processing 'frame-ancestors').
  if (delegate_ && !context_frame)
    delegate_->DispatchViolationEvent(*violation_data, element);

  AuditsIssue audits_issue = AuditsIssue::CreateContentSecurityPolicyIssue(
      *violation_data, header_type == ContentSecurityPolicyType::kReport,
      violation_type, context_frame, element, source_location, issue_id);

  if (context_frame) {
    context_frame->DomWindow()->AddInspectorIssue(std::move(audits_issue));
  } else if (delegate_) {
    delegate_->AddInspectorIssue(std::move(audits_issue));
  }
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
  csp_report->SetString("disposition",
                        violation_data->disposition().AsString());
  csp_report->SetString("blocked-uri", violation_data->blockedURI());
  if (violation_data->lineNumber())
    csp_report->SetInteger("line-number", violation_data->lineNumber());
  if (violation_data->columnNumber())
    csp_report->SetInteger("column-number", violation_data->columnNumber());
  if (!violation_data->sourceFile().empty())
    csp_report->SetString("source-file", violation_data->sourceFile());
  csp_report->SetInteger("status-code", violation_data->statusCode());

  csp_report->SetString("script-sample", violation_data->sample());
  if (violation_data->hasEvalHash() && !violation_data->evalHash().empty()) {
    csp_report->SetString("eval-hash", violation_data->evalHash());
  }

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
      ReportViolation(
          GetDirectiveName(CSPDirectiveName::BlockAllMixedContent),
          CSPDirectiveName::BlockAllMixedContent, String(), blocked_url,
          policy->report_endpoints, policy->use_reporting_api,
          policy->header->header_value, policy->header->type,
          ContentSecurityPolicyViolationType::kURLViolation, nullptr,
          /*contextFrame=*/nullptr);
    }
  }
}

void ContentSecurityPolicy::ReportReportOnlyInMeta(const String& header) {
  LogToConsole(StrCat({"The report-only Content Security Policy '", header,
                       "' was delivered via a <meta> element, which is "
                       "disallowed. The policy has been ignored."}));
}

void ContentSecurityPolicy::ReportMetaOutsideHead(const String& header) {
  LogToConsole(
      StrCat({"The Content Security Policy '", header,
              "' was delivered via a <meta> element outside the document's "
              "<head>, which is disallowed. The policy has been ignored."}));
}

void ContentSecurityPolicy::LogToConsole(const String& message,
                                         mojom::ConsoleMessageLevel level) {
  LogToConsole(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity, level, message));
}

mojom::blink::ContentSecurityPolicyViolationType
ContentSecurityPolicy::BuildCSPViolationType(
    ContentSecurityPolicyViolationType violation_type) {
  switch (violation_type) {
    case blink::ContentSecurityPolicyViolationType::kEvalViolation:
      return mojom::blink::ContentSecurityPolicyViolationType::kEvalViolation;
    case blink::ContentSecurityPolicyViolationType::kWasmEvalViolation:
      return mojom::blink::ContentSecurityPolicyViolationType::
          kWasmEvalViolation;
    case blink::ContentSecurityPolicyViolationType::kInlineViolation:
      return mojom::blink::ContentSecurityPolicyViolationType::kInlineViolation;
    case blink::ContentSecurityPolicyViolationType::
        kTrustedTypesPolicyViolation:
      return mojom::blink::ContentSecurityPolicyViolationType::
          kTrustedTypesPolicyViolation;
    case blink::ContentSecurityPolicyViolationType::kTrustedTypesSinkViolation:
      return mojom::blink::ContentSecurityPolicyViolationType::
          kTrustedTypesSinkViolation;
    case blink::ContentSecurityPolicyViolationType::kURLViolation:
      return mojom::blink::ContentSecurityPolicyViolationType::kURLViolation;
    case blink::ContentSecurityPolicyViolationType::kSRIViolation:
      return mojom::blink::ContentSecurityPolicyViolationType::kSRIViolation;
  }
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

// static
bool ContentSecurityPolicy::ShouldBypassMainWorldDeprecated(
    const ExecutionContext* context) {
  if (!context)
    return false;

  return ShouldBypassMainWorldDeprecated(context->GetCurrentWorld());
}

// static
bool ContentSecurityPolicy::ShouldBypassMainWorldDeprecated(
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
    case CSPDirectiveName::FencedFrameSrc:
      return "fenced-frame-src";
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
    case CSPDirectiveName::ObjectSrc:
      return "object-src";
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
    case CSPDirectiveName::ScriptSrcV2:
      return "script-src-v2";
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
  }

  NOTREACHED();
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
  if (name == "fenced-frame-src")
    return CSPDirectiveName::FencedFrameSrc;
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
  if (name == "object-src")
    return CSPDirectiveName::ObjectSrc;
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
  if (base::FeatureList::IsEnabled(network::features::kCSPScriptSrcV2) &&
      name == "script-src-v2") {
    return CSPDirectiveName::ScriptSrcV2;
  }
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

const Vector<network::mojom::blink::ContentSecurityPolicyPtr>&
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

bool ContentSecurityPolicy::AllowFencedFrameOpaqueURL() const {
  for (const auto& policy : GetParsedPolicies()) {
    if (!AllowOpaqueFencedFrames(policy)) {
      return false;
    }
  }
  return true;
}

bool ContentSecurityPolicy::HasEnforceFrameAncestorsDirectives() {
  return std::ranges::any_of(policies_, [](const auto& csp) {
    return csp->header->type ==
               network::mojom::ContentSecurityPolicyType::kEnforce &&
           csp->directives.Contains(
               network::mojom::CSPDirectiveName::FrameAncestors);
  });
}

void ContentSecurityPolicy::Count(WebFeature feature) const {
  if (delegate_)
    delegate_->Count(feature);
}

}  // namespace blink
