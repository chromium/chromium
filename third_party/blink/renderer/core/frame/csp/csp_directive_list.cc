// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"

#include <memory>
#include <utility>

#include "base/notreached.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy_violation_type.h"
#include "third_party/blink/renderer/core/frame/csp/source_list_directive.h"
#include "third_party/blink/renderer/core/frame/csp/trusted_types_directive.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using network::mojom::ContentSecurityPolicySource;
using network::mojom::ContentSecurityPolicyType;

namespace {

String GetRawDirectiveForMessage(
    const HashMap<CSPDirectiveName, String> raw_directives,
    CSPDirectiveName directive_name) {
  StringBuilder builder;
  builder.Append(ContentSecurityPolicy::GetDirectiveName(directive_name));
  builder.Append(" ");
  builder.Append(raw_directives.at(directive_name));
  return builder.ToString();
}

String GetSha256String(const String& content) {
  DigestValue digest;
  StringUTF8Adaptor utf8_content(content);
  bool digest_success = ComputeDigest(kHashAlgorithmSha256,
                                      base::as_byte_span(utf8_content), digest);
  if (!digest_success) {
    return "sha256-...";
  }

  return "sha256-" + Base64Encode(digest);
}

network::mojom::blink::CSPHashAlgorithm ConvertHashAlgorithmToCSPHashAlgorithm(
    IntegrityAlgorithm algorithm) {
  // TODO(antoniosartori): Consider merging these two enums.
  switch (algorithm) {
    case IntegrityAlgorithm::kSha256:
      return network::mojom::blink::CSPHashAlgorithm::SHA256;
    case IntegrityAlgorithm::kSha384:
      return network::mojom::blink::CSPHashAlgorithm::SHA384;
    case IntegrityAlgorithm::kSha512:
      return network::mojom::blink::CSPHashAlgorithm::SHA512;
  }
  NOTREACHED_IN_MIGRATION();
  return network::mojom::blink::CSPHashAlgorithm::None;
}

// IntegrityMetadata (from SRI) has base64-encoded digest values, but CSP uses
// binary format. This converts from the former to the latter.
bool ParseBase64Digest(String base64, Vector<uint8_t>& hash) {
  DCHECK(hash.empty());

  // We accept base64url-encoded data here by normalizing it to base64.
  Vector<char> out;
  if (!Base64Decode(NormalizeToBase64(base64), out))
    return false;
  if (out.empty() || out.size() > kMaxDigestSize)
    return false;
  for (char el : out)
    hash.push_back(el);
  return true;
}

// https://w3c.github.io/webappsec-csp/#effective-directive-for-inline-check
CSPDirectiveName EffectiveDirectiveForInlineCheck(
    ContentSecurityPolicy::InlineType inline_type) {
  // 1. Switch on type: [spec text]
  switch (inline_type) {
    // "script":
    // "navigation":
    // 1. Return script-src-elem. [spec text]
    case ContentSecurityPolicy::InlineType::kScript:
    case ContentSecurityPolicy::InlineType::kScriptSpeculationRules:
    case ContentSecurityPolicy::InlineType::kNavigation:
      return CSPDirectiveName::ScriptSrcElem;

    // "script attribute":
    // 1. Return script-src-attr. [spec text]
    case ContentSecurityPolicy::InlineType::kScriptAttribute:
      return CSPDirectiveName::ScriptSrcAttr;

    // "style":
    // 1. Return style-src-elem. [spec text]
    case ContentSecurityPolicy::InlineType::kStyle:
      return CSPDirectiveName::StyleSrcElem;

    // "style attribute":
    // 1. Return style-src-attr. [spec text]
    case ContentSecurityPolicy::InlineType::kStyleAttribute:
      return CSPDirectiveName::StyleSrcAttr;
  }
}

CSPOperativeDirective OperativeDirective(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    CSPDirectiveName type,
    CSPDirectiveName original_type = CSPDirectiveName::Unknown) {
  if (type == CSPDirectiveName::Unknown) {
    return CSPOperativeDirective{CSPDirectiveName::Unknown, nullptr};
  }

  if (original_type == CSPDirectiveName::Unknown) {
    original_type = type;
  }

  const auto directive = csp.directives.find(type);

  // If the directive does not exist, rely on the fallback directive.
  return (directive != csp.directives.end())
             ? CSPOperativeDirective{type, directive->value.get()}
             : OperativeDirective(
                   csp, network::CSPFallbackDirective(type, original_type),
                   original_type);
}

void ReportViolation(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    const String& directive_text,
    CSPDirectiveName effective_type,
    const String& console_message,
    const KURL& blocked_url,
    ContentSecurityPolicyViolationType violation_type =
        ContentSecurityPolicyViolationType::kURLViolation,
    const String& sample = String(),
    const String& sample_prefix = String(),
    std::optional<base::UnguessableToken> issue_id = std::nullopt) {
  String message = CSPDirectiveListIsReportOnly(csp)
                       ? "[Report Only] " + console_message
                       : console_message;
  policy->LogToConsole(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError, message));
  policy->ReportViolation(directive_text, effective_type, message, blocked_url,
                          csp.report_endpoints, csp.use_reporting_api,
                          csp.header->header_value, csp.header->type,
                          violation_type, std::unique_ptr<SourceLocation>(),
                          nullptr,  // localFrame
                          nullptr,  // Element*
                          sample, sample_prefix, issue_id);
}

void ReportViolationWithLocation(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    const String& directive_text,
    CSPDirectiveName effective_type,
    const String& console_message,
    const KURL& blocked_url,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    Element* element,
    const String& source) {
  String message = CSPDirectiveListIsReportOnly(csp)
                       ? "[Report Only] " + console_message
                       : console_message;
  std::unique_ptr<SourceLocation> source_location =
      CaptureSourceLocation(context_url, context_line.OneBasedInt(), 0);
  policy->LogToConsole(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError, message, source_location->Clone()));
  policy->ReportViolation(directive_text, effective_type, message, blocked_url,
                          csp.report_endpoints, csp.use_reporting_api,
                          csp.header->header_value, csp.header->type,
                          ContentSecurityPolicyViolationType::kInlineViolation,
                          std::move(source_location), nullptr,  // localFrame
                          element, source);
}

void ReportEvalViolation(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    const String& directive_text,
    CSPDirectiveName effective_type,
    const String& message,
    const KURL& blocked_url,
    const ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) {
  String report_message =
      CSPDirectiveListIsReportOnly(csp) ? "[Report Only] " + message : message;
  // Print a console message if it won't be redundant with a
  // JavaScript exception that the caller will throw. (Exceptions will
  // never get thrown in report-only mode because the caller won't see
  // a violation.)
  if (CSPDirectiveListIsReportOnly(csp) ||
      exception_status == ContentSecurityPolicy::kWillNotThrowException) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError, report_message);
    policy->LogToConsole(console_message);
  }
  policy->ReportViolation(
      directive_text, effective_type, message, blocked_url,
      csp.report_endpoints, csp.use_reporting_api, csp.header->header_value,
      csp.header->type, ContentSecurityPolicyViolationType::kEvalViolation,
      std::unique_ptr<SourceLocation>(), nullptr, nullptr, content);
}

void ReportWasmEvalViolation(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    const String& directive_text,
    CSPDirectiveName effective_type,
    const String& message,
    const KURL& blocked_url,
    const ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) {
  String report_message =
      CSPDirectiveListIsReportOnly(csp) ? "[Report Only] " + message : message;
  // Print a console message if it won't be redundant with a JavaScript
  // exception that the caller will throw. Exceptions will never get thrown in
  // report-only mode because the caller won't see a violation.
  if (CSPDirectiveListIsReportOnly(csp) ||
      exception_status == ContentSecurityPolicy::kWillNotThrowException) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity,
        mojom::blink::ConsoleMessageLevel::kError, report_message);
    policy->LogToConsole(console_message);
  }
  policy->ReportViolation(
      directive_text, effective_type, message, blocked_url,
      csp.report_endpoints, csp.use_reporting_api, csp.header->header_value,
      csp.header->type, ContentSecurityPolicyViolationType::kWasmEvalViolation,
      std::unique_ptr<SourceLocation>(), nullptr, nullptr, content);
}

bool CheckEval(const network::mojom::blink::CSPSourceList* directive) {
  return !directive || directive->allow_eval;
}

bool SupportsWasmEval(const network::mojom::blink::ContentSecurityPolicy& csp,
                      const ContentSecurityPolicy* policy) {
  return policy->SupportsWasmEval() ||
         SchemeRegistry::SchemeSupportsWasmEvalCSP(csp.self_origin->scheme);
}

bool CheckWasmEval(const network::mojom::blink::ContentSecurityPolicy& csp,
                   const ContentSecurityPolicy* policy) {
  const network::mojom::blink::CSPSourceList* directive =
      OperativeDirective(csp, CSPDirectiveName::ScriptSrc).source_list;
  return !directive || directive->allow_eval ||
         (SupportsWasmEval(csp, policy) && directive->allow_wasm_eval) ||
         directive->allow_wasm_unsafe_eval;
}

bool CheckHash(const network::mojom::blink::CSPSourceList* directive,
               const network::mojom::blink::CSPHashSource& hash_value) {
  return !directive || CSPSourceListAllowHash(*directive, hash_value);
}

bool CheckUnsafeHashesAllowed(
    const network::mojom::blink::CSPSourceList* directive) {
  return !directive || directive->allow_unsafe_hashes;
}

bool CheckUnsafeHashesAllowed(
    ContentSecurityPolicy::InlineType inline_type,
    const network::mojom::blink::CSPSourceList* directive) {
  switch (inline_type) {
    case ContentSecurityPolicy::InlineType::kNavigation:
    case ContentSecurityPolicy::InlineType::kScriptAttribute:
    case ContentSecurityPolicy::InlineType::kStyleAttribute:
      return CheckUnsafeHashesAllowed(directive);

    case ContentSecurityPolicy::InlineType::kScript:
    case ContentSecurityPolicy::InlineType::kScriptSpeculationRules:
    case ContentSecurityPolicy::InlineType::kStyle:
      return true;
  }
}

bool CheckDynamic(const network::mojom::blink::CSPSourceList* directive,
                  CSPDirectiveName effective_type) {
  // 'strict-dynamic' only applies to scripts
  if (effective_type != CSPDirectiveName::ScriptSrc &&
      effective_type != CSPDirectiveName::ScriptSrcAttr &&
      effective_type != CSPDirectiveName::ScriptSrcElem &&
      effective_type != CSPDirectiveName::WorkerSrc) {
    return false;
  }
  return !directive || directive->allow_dynamic;
}

bool IsMatchingNoncePresent(
    const network::mojom::blink::CSPSourceList* directive,
    const String& nonce) {
  return directive && CSPSourceListAllowNonce(*directive, nonce);
}

bool AreAllMatchingHashesPresent(
    const network::mojom::blink::CSPSourceList* directive,
    const IntegrityMetadataSet& hashes) {
  if (!directive || hashes.empty())
    return false;
  for (const std::pair<String, IntegrityAlgorithm>& hash : hashes) {
    // Convert the hash from integrity metadata format to CSP format.
    network::mojom::blink::CSPHashSourcePtr csp_hash =
        network::mojom::blink::CSPHashSource::New();
    csp_hash->algorithm = ConvertHashAlgorithmToCSPHashAlgorithm(hash.second);
    if (!ParseBase64Digest(hash.first, csp_hash->value))
      return false;
    // All integrity hashes must be listed in the CSP.
    if (!CSPSourceListAllowHash(*directive, *csp_hash))
      return false;
  }
  return true;
}

bool CheckEvalAndReportViolation(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    const String& console_message,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) {
  CSPOperativeDirective directive =
      OperativeDirective(csp, CSPDirectiveName::ScriptSrc);
  if (CheckEval(directive.source_list))
    return true;

  String suffix = String();
  if (directive.type == CSPDirectiveName::DefaultSrc) {
    suffix =
        " Note that 'script-src' was not explicitly set, so 'default-src' is "
        "used as a fallback.";
  }

  String raw_directive =
      GetRawDirectiveForMessage(csp.raw_directives, directive.type);
  ReportEvalViolation(
      csp, policy, raw_directive, CSPDirectiveName::ScriptSrc,
      console_message + "\"" + raw_directive + "\"." + suffix + "\n", KURL(),
      exception_status,
      directive.source_list->report_sample ? content : g_empty_string);
  if (!CSPDirectiveListIsReportOnly(csp)) {
    policy->ReportBlockedScriptExecutionToInspector(raw_directive);
    return false;
  }
  return true;
}

bool CheckWasmEvalAndReportViolation(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    const String& console_message,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) {
  if (CheckWasmEval(csp, policy))
    return true;

  CSPOperativeDirective directive =
      OperativeDirective(csp, CSPDirectiveName::ScriptSrc);
  String suffix = String();
  if (directive.type == CSPDirectiveName::DefaultSrc) {
    suffix =
        " Note that 'script-src' was not explicitly set, so 'default-src' is "
        "used as a fallback.";
  }

  String raw_directive =
      GetRawDirectiveForMessage(csp.raw_directives, directive.type);
  ReportWasmEvalViolation(
      csp, policy, raw_directive, CSPDirectiveName::ScriptSrc,
      console_message + "\"" + raw_directive + "\"." + suffix + "\n", KURL(),
      exception_status,
      directive.source_list->report_sample ? content : g_empty_string);
  if (!CSPDirectiveListIsReportOnly(csp)) {
    policy->ReportBlockedScriptExecutionToInspector(raw_directive);
    return false;
  }
  return true;
}

bool CheckInlineAndReportViolation(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    CSPOperativeDirective directive,
    const String& console_message,
    Element* element,
    const String& source,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    ContentSecurityPolicy::InlineType inline_type,
    const String& hash_value,
    CSPDirectiveName effective_type) {
  if (!directive.source_list ||
      CSPSourceListAllowAllInline(directive.type, inline_type,
                                  *directive.source_list)) {
    return true;
  }

  bool is_script = ContentSecurityPolicy::IsScriptInlineType(inline_type);

  String suffix = String();
  if (directive.source_list->allow_inline &&
      CSPSourceListIsHashOrNoncePresent(*directive.source_list)) {
    // If inline is allowed, but a hash or nonce is present, we ignore
    // 'unsafe-inline'. Throw a reasonable error.
    suffix =
        " Note that 'unsafe-inline' is ignored if either a hash or nonce value "
        "is present in the source list.";
  } else {
    suffix =
        " Either the 'unsafe-inline' keyword, a hash ('" + hash_value +
        "'), or a nonce ('nonce-...') is required to enable inline execution.";

    if (!CheckUnsafeHashesAllowed(inline_type, directive.source_list)) {
      suffix = suffix +
               " Note that hashes do not apply to event handlers, style "
               "attributes and javascript: navigations unless the "
               "'unsafe-hashes' keyword is present.";
    }

    if (directive.type == CSPDirectiveName::DefaultSrc) {
      suffix = suffix + " Note also that '" +
               String(is_script ? "script" : "style") +
               "-src' was not explicitly set, so 'default-src' is used as a "
               "fallback.";
    }
  }

  String raw_directive =
      GetRawDirectiveForMessage(csp.raw_directives, directive.type);
  ReportViolationWithLocation(
      csp, policy, raw_directive, effective_type,
      console_message + "\"" + raw_directive + "\"." + suffix + "\n", KURL(),
      context_url, context_line, element,
      directive.source_list->report_sample ? source : g_empty_string);

  if (!CSPDirectiveListIsReportOnly(csp)) {
    if (is_script)
      policy->ReportBlockedScriptExecutionToInspector(raw_directive);
    return false;
  }
  return true;
}

void ReportViolationForCheckSource(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    CSPOperativeDirective directive,
    const KURL& url,
    CSPDirectiveName effective_type,
    const KURL& url_before_redirects,
    String suffix) {
  // We should never have a violation against `child-src`
  // directly; the effective directive should always be one of the explicit
  // fetch directives, or default-src in the case of resource hints.
  DCHECK_NE(CSPDirectiveName::ChildSrc, effective_type);

  String prefix = "Refused to ";
  switch (effective_type) {
    case CSPDirectiveName::BaseURI:
      prefix = prefix + "set the document's base URI to '";
      break;
    case CSPDirectiveName::ConnectSrc:
      prefix = prefix + "connect to '";
      break;
    case CSPDirectiveName::DefaultSrc:
      // This would occur if we try to fetch content without an explicit
      // destination - i.e. resource hints (prefetch, preconnect).
      prefix = prefix + "fetch content from '";
      break;
    case CSPDirectiveName::FontSrc:
      prefix = prefix + "load the font '";
      break;
    case CSPDirectiveName::FormAction:
      prefix = prefix + "send form data to '";
      break;
    case CSPDirectiveName::ImgSrc:
      prefix = prefix + "load the image '";
      break;
    case CSPDirectiveName::ManifestSrc:
      prefix = prefix + "load manifest from '";
      break;
    case CSPDirectiveName::MediaSrc:
      prefix = prefix + "load media from '";
      break;
    case CSPDirectiveName::ObjectSrc:
      prefix = prefix + "load plugin data from '";
      break;
    case CSPDirectiveName::ScriptSrc:
    case CSPDirectiveName::ScriptSrcAttr:
    case CSPDirectiveName::ScriptSrcElem:
      prefix = prefix + "load the script '";
      break;
    case CSPDirectiveName::StyleSrc:
    case CSPDirectiveName::StyleSrcAttr:
    case CSPDirectiveName::StyleSrcElem:
      prefix = prefix + "load the stylesheet '";
      break;
    case CSPDirectiveName::WorkerSrc:
      prefix = prefix + "create a worker from '";
      break;
    case CSPDirectiveName::BlockAllMixedContent:
    case CSPDirectiveName::ChildSrc:
    case CSPDirectiveName::FencedFrameSrc:
    case CSPDirectiveName::FrameAncestors:
    case CSPDirectiveName::FrameSrc:
    case CSPDirectiveName::ReportTo:
    case CSPDirectiveName::ReportURI:
    case CSPDirectiveName::RequireTrustedTypesFor:
    case CSPDirectiveName::Sandbox:
    case CSPDirectiveName::TreatAsPublicAddress:
    case CSPDirectiveName::TrustedTypes:
    case CSPDirectiveName::UpgradeInsecureRequests:
    case CSPDirectiveName::Unknown:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  String directive_name =
      ContentSecurityPolicy::GetDirectiveName(directive.type);
  String effective_directive_name =
      ContentSecurityPolicy::GetDirectiveName(effective_type);
  if (directive_name != effective_directive_name) {
    suffix = suffix + " Note that '" + effective_directive_name +
             "' was not explicitly set, so '" + directive_name +
             "' is used as a fallback.";
  }

  // Wildcards match network schemes ('http', 'https', 'ws', 'wss'), and the
  // scheme of the protected resource:
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression.
  // Other schemes, including custom schemes, must be explicitly listed in a
  // source list.
  if (directive.source_list->allow_star) {
    suffix = suffix +
             " Note that '*' matches only URLs with network schemes ('http', "
             "'https', 'ws', 'wss'), or URLs whose scheme matches `self`'s "
             "scheme. The scheme '" +
             url.Protocol() + ":' must be added explicitly.";
  }

  String raw_directive =
      GetRawDirectiveForMessage(csp.raw_directives, directive.type);
  ReportViolation(csp, policy, raw_directive, effective_type,
                  prefix + url.ElidedString() +
                      "' because it violates the following Content Security "
                      "Policy directive: \"" +
                      raw_directive + "\"." + suffix + "\n",
                  url_before_redirects);
}

CSPCheckResult CheckSource(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    CSPOperativeDirective directive,
    const KURL& url,
    CSPDirectiveName effective_type,
    const KURL& url_before_redirects,
    ResourceRequest::RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition) {
  if (!directive.source_list) {
    return CSPCheckResult::Allowed();
  }

  // If |url| is empty, fall back to the policy URL to ensure that
  // <object>'s without a `src` can be blocked/allowed, as they can
  // still load plugins even though they don't actually have a URL.
  const KURL& url_to_check =
      url.IsEmpty() ? policy->FallbackUrlForPlugin() : url;
  String suffix = String();
  CSPCheckResult result = CSPSourceListAllows(
      *directive.source_list, *csp.self_origin, url_to_check, redirect_status);
  if (result) {
    // We ignore URL-based allowlists if we're allowing dynamic script
    // injection.
    if (!CheckDynamic(directive.source_list, effective_type)) {
      return result;
    } else {
      suffix =
          " Note that 'strict-dynamic' is present, so host-based allowlisting "
          "is disabled.";
    }
  }

  if (reporting_disposition == ReportingDisposition::kReport) {
    ReportViolationForCheckSource(csp, policy, directive, url, effective_type,
                                  url_before_redirects, suffix);
  }

  return CSPCheckResult(CSPDirectiveListIsReportOnly(csp));
}

bool AllowDynamicWorker(
    const network::mojom::blink::ContentSecurityPolicy& csp) {
  const network::mojom::blink::CSPSourceList* worker_src =
      OperativeDirective(csp, CSPDirectiveName::WorkerSrc).source_list;
  return CheckDynamic(worker_src, CSPDirectiveName::WorkerSrc);
}

}  // namespace

bool CSPDirectiveListIsReportOnly(
    const network::mojom::blink::ContentSecurityPolicy& csp) {
  return csp.header->type == network::mojom::ContentSecurityPolicyType::kReport;
}

bool CSPDirectiveListAllowTrustedTypeAssignmentFailure(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    const String& message,
    const String& sample,
    const String& sample_prefix,
    std::optional<base::UnguessableToken> issue_id) {
  if (!CSPDirectiveListRequiresTrustedTypes(csp))
    return true;

  ReportViolation(
      csp, policy,
      ContentSecurityPolicy::GetDirectiveName(
          CSPDirectiveName::RequireTrustedTypesFor),
      CSPDirectiveName::RequireTrustedTypesFor, message, KURL(),
      ContentSecurityPolicyViolationType::kTrustedTypesSinkViolation, sample,
      sample_prefix, issue_id);
  return CSPDirectiveListIsReportOnly(csp);
}

bool CSPDirectiveListAllowInline(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    ContentSecurityPolicy::InlineType inline_type,
    Element* element,
    const String& content,
    const String& nonce,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    ReportingDisposition reporting_disposition) {
  CSPDirectiveName type = EffectiveDirectiveForInlineCheck(inline_type);

  CSPOperativeDirective directive = OperativeDirective(csp, type);
  if (IsMatchingNoncePresent(directive.source_list, nonce))
    return true;

  auto* html_script_element = DynamicTo<HTMLScriptElement>(element);
  if (html_script_element &&
      (inline_type == ContentSecurityPolicy::InlineType::kScript ||
       inline_type ==
           ContentSecurityPolicy::InlineType::kScriptSpeculationRules) &&
      !html_script_element->Loader()->IsParserInserted() &&
      CSPDirectiveListAllowDynamic(csp, type)) {
    return true;
  }
  if (reporting_disposition == ReportingDisposition::kReport) {
    String hash_value;
    switch (inline_type) {
      case ContentSecurityPolicy::InlineType::kNavigation:
      case ContentSecurityPolicy::InlineType::kScriptAttribute:
        hash_value = "sha256-...";
        break;

      case ContentSecurityPolicy::InlineType::kScript:
      case ContentSecurityPolicy::InlineType::kScriptSpeculationRules:
      case ContentSecurityPolicy::InlineType::kStyleAttribute:
      case ContentSecurityPolicy::InlineType::kStyle:
        hash_value = GetSha256String(content);
        break;
    }

    String message;
    switch (inline_type) {
      case ContentSecurityPolicy::InlineType::kNavigation:
        message = "run the JavaScript URL";
        break;

      case ContentSecurityPolicy::InlineType::kScriptSpeculationRules:
        message = "apply inline speculation rules";
        break;

      case ContentSecurityPolicy::InlineType::kScriptAttribute:
        message = "execute inline event handler";
        break;

      case ContentSecurityPolicy::InlineType::kScript:
        message = "execute inline script";
        break;

      case ContentSecurityPolicy::InlineType::kStyleAttribute:
      case ContentSecurityPolicy::InlineType::kStyle:
        message = "apply inline style";
        break;
    }

    return CheckInlineAndReportViolation(
        csp, policy, directive,
        "Refused to " + message +
            " because it violates the following Content Security Policy "
            "directive: ",
        element, content, context_url, context_line, inline_type, hash_value,
        type);
  }

  return !directive.source_list ||
         CSPSourceListAllowAllInline(directive.type, inline_type,
                                     *directive.source_list);
}

bool CSPDirectiveListShouldCheckEval(
    const network::mojom::blink::ContentSecurityPolicy& csp) {
  return !CheckEval(
      OperativeDirective(csp, CSPDirectiveName::ScriptSrc).source_list);
}

bool CSPDirectiveListAllowEval(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) {
  if (reporting_disposition == ReportingDisposition::kReport) {
    return CheckEvalAndReportViolation(
        csp, policy,
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: ",
        exception_status, content);
  }
  return CSPDirectiveListIsReportOnly(csp) ||
         CheckEval(
             OperativeDirective(csp, CSPDirectiveName::ScriptSrc).source_list);
}

// Complex conditional around infix is temp, until SupportsWasmEval goes away.
bool CSPDirectiveListAllowWasmCodeGeneration(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) {
  if (reporting_disposition == ReportingDisposition::kReport) {
    String infix = SupportsWasmEval(csp, policy)
                       ? "neither 'wasm-eval' nor 'unsafe-eval' is"
                       : "'unsafe-eval' is not";
    return CheckWasmEvalAndReportViolation(
        csp, policy,
        "Refused to compile or instantiate WebAssembly module because " +
            infix +
            " an allowed source of script in the following "
            "Content Security Policy directive: ",
        exception_status, content);
  }
  return CSPDirectiveListIsReportOnly(csp) || CheckWasmEval(csp, policy);
}

bool CSPDirectiveListShouldDisableEval(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    String& error_message) {
  CSPOperativeDirective directive =
      OperativeDirective(csp, CSPDirectiveName::ScriptSrc);
  if (!CheckEval(directive.source_list)) {
    error_message =
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: \"" +
        GetRawDirectiveForMessage(csp.raw_directives, directive.type) + "\".\n";
    return true;
  } else if (CSPDirectiveListRequiresTrustedTypes(csp)) {
    error_message =
        "Refused to evaluate a string as JavaScript because this document "
        "requires 'Trusted Type' assignment.";
    return true;
  }
  return false;
}

bool CSPDirectiveListShouldDisableWasmEval(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    const ContentSecurityPolicy* policy,
    String& error_message) {
  if (CheckWasmEval(csp, policy)) {
    return false;
  }

  const char* format =
      SupportsWasmEval(csp, policy)
          ? "Refused to compile or instantiate WebAssembly module because "
            "neither 'wasm-eval' nor 'unsafe-eval' is an allowed source of "
            "script in the following Content Security Policy directive: \"%s\""
          : "Refused to compile or instantiate WebAssembly module because "
            "'unsafe-eval' is not an allowed source of script in the following "
            "Content Security Policy directive: \"%s\"";

  CSPOperativeDirective directive =
      OperativeDirective(csp, CSPDirectiveName::ScriptSrc);
  error_message = String::Format(
      format, GetRawDirectiveForMessage(csp.raw_directives, directive.type)
                  .Ascii()
                  .c_str());
  return true;
}

CSPCheckResult CSPDirectiveListAllowFromSource(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    CSPDirectiveName type,
    const KURL& url,
    const KURL& url_before_redirects,
    ResourceRequest::RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    const String& nonce,
    const IntegrityMetadataSet& hashes,
    ParserDisposition parser_disposition) {
  DCHECK(type == CSPDirectiveName::BaseURI ||
         type == CSPDirectiveName::ConnectSrc ||
         type == CSPDirectiveName::DefaultSrc ||
         type == CSPDirectiveName::FontSrc ||
         type == CSPDirectiveName::FormAction ||
         // FrameSrc and ChildSrc enabled here only for the resource hint check
         type == CSPDirectiveName::ChildSrc ||
         type == CSPDirectiveName::FrameSrc ||
         type == CSPDirectiveName::ImgSrc ||
         type == CSPDirectiveName::ManifestSrc ||
         type == CSPDirectiveName::MediaSrc ||
         type == CSPDirectiveName::ObjectSrc ||
         type == CSPDirectiveName::ScriptSrc ||
         type == CSPDirectiveName::ScriptSrcElem ||
         type == CSPDirectiveName::StyleSrc ||
         type == CSPDirectiveName::StyleSrcElem ||
         type == CSPDirectiveName::WorkerSrc);

  if (type == CSPDirectiveName::ObjectSrc) {
    if (url.ProtocolIsAbout()) {
      return CSPCheckResult::Allowed();
    }
  }

  if (type == CSPDirectiveName::WorkerSrc && AllowDynamicWorker(csp)) {
    return CSPCheckResult::Allowed();
  }

  if (type == CSPDirectiveName::ScriptSrcElem ||
      type == CSPDirectiveName::StyleSrcElem) {
    if (IsMatchingNoncePresent(OperativeDirective(csp, type).source_list,
                               nonce)) {
      return CSPCheckResult::Allowed();
    }
  }

  if (type == CSPDirectiveName::ScriptSrcElem) {
    if (parser_disposition == kNotParserInserted &&
        CSPDirectiveListAllowDynamic(csp, type)) {
      return CSPCheckResult::Allowed();
    }
    if (AreAllMatchingHashesPresent(OperativeDirective(csp, type).source_list,
                                    hashes)) {
      return CSPCheckResult::Allowed();
    }
  }

  CSPOperativeDirective directive = OperativeDirective(csp, type);
  return CheckSource(csp, policy, directive, url, type, url_before_redirects,
                     redirect_status, reporting_disposition);
}

bool CSPDirectiveListAllowTrustedTypePolicy(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    const String& policy_name,
    bool is_duplicate,
    ContentSecurityPolicy::AllowTrustedTypePolicyDetails& violation_details,
    std::optional<base::UnguessableToken> issue_id) {
  if (!csp.trusted_types ||
      CSPTrustedTypesAllows(*csp.trusted_types, policy_name, is_duplicate,
                            violation_details)) {
    return true;
  }

  String raw_directive = GetRawDirectiveForMessage(
      csp.raw_directives,
      network::mojom::blink::CSPDirectiveName::TrustedTypes);
  const char* message =
      (violation_details == ContentSecurityPolicy::kDisallowedDuplicateName)
          ? "Refused to create a TrustedTypePolicy named '%s' because a "
            "policy with that name already exists and the Content Security "
            "Policy directive does not 'allow-duplicates': \"%s\"."
          : "Refused to create a TrustedTypePolicy named '%s' because "
            "it violates the following Content Security Policy directive: "
            "\"%s\".";
  ReportViolation(
      csp, policy, "trusted-types", CSPDirectiveName::TrustedTypes,
      String::Format(message, policy_name.Utf8().c_str(),
                     raw_directive.Utf8().c_str()),
      KURL(), ContentSecurityPolicyViolationType::kTrustedTypesPolicyViolation,
      policy_name, String(), issue_id);

  return CSPDirectiveListIsReportOnly(csp);
}

bool CSPDirectiveListRequiresTrustedTypes(
    const network::mojom::blink::ContentSecurityPolicy& csp) {
  return csp.require_trusted_types_for ==
         network::mojom::blink::CSPRequireTrustedTypesFor::Script;
}

bool CSPDirectiveListAllowHash(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    const network::mojom::blink::CSPHashSource& hash_value,
    const ContentSecurityPolicy::InlineType inline_type) {
  CSPDirectiveName directive_type =
      EffectiveDirectiveForInlineCheck(inline_type);
  const network::mojom::blink::CSPSourceList* operative_directive =
      OperativeDirective(csp, directive_type).source_list;

  // https://w3c.github.io/webappsec-csp/#match-element-to-source-list
  // Step 5. If type is "script" or "style", or unsafe-hashes flag is true:
  // [spec text]
  return CheckUnsafeHashesAllowed(inline_type, operative_directive) &&
         CheckHash(operative_directive, hash_value);
}

bool CSPDirectiveListAllowDynamic(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    CSPDirectiveName directive_type) {
  return CheckDynamic(OperativeDirective(csp, directive_type).source_list,
                      directive_type);
}

bool CSPDirectiveListIsObjectRestrictionReasonable(
    const network::mojom::blink::ContentSecurityPolicy& csp) {
  const network::mojom::blink::CSPSourceList* object_src =
      OperativeDirective(csp, CSPDirectiveName::ObjectSrc).source_list;
  return object_src && CSPSourceListIsNone(*object_src);
}

bool CSPDirectiveListIsBaseRestrictionReasonable(
    const network::mojom::blink::ContentSecurityPolicy& csp) {
  const auto base_uri = csp.directives.find(CSPDirectiveName::BaseURI);
  return (base_uri != csp.directives.end()) &&
         (CSPSourceListIsNone(*base_uri->value) ||
          CSPSourceListIsSelf(*base_uri->value));
}

bool CSPDirectiveListIsScriptRestrictionReasonable(
    const network::mojom::blink::ContentSecurityPolicy& csp) {
  CSPOperativeDirective script_src =
      OperativeDirective(csp, CSPDirectiveName::ScriptSrc);

  // If no `script-src` enforcement occurs, or it allows any and all inline
  // script, the restriction is not reasonable.
  if (!script_src.source_list ||
      CSPSourceListAllowAllInline(script_src.type,
                                  ContentSecurityPolicy::InlineType::kScript,
                                  *script_src.source_list)) {
    return false;
  }

  if (CSPSourceListIsNone(*script_src.source_list))
    return true;

  // Policies containing `'strict-dynamic'` are reasonable, as that keyword
  // ensures that host-based expressions and `'unsafe-inline'` are ignored.
  return CSPSourceListIsHashOrNoncePresent(*script_src.source_list) &&
         (script_src.source_list->allow_dynamic ||
          !CSPSourceListAllowsURLBasedMatching(*script_src.source_list));
}

bool CSPDirectiveListIsActiveForConnections(
    const network::mojom::blink::ContentSecurityPolicy& csp) {
  return OperativeDirective(csp, CSPDirectiveName::ConnectSrc).source_list;
}

CSPOperativeDirective CSPDirectiveListOperativeDirective(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    CSPDirectiveName type) {
  return OperativeDirective(csp, type);
}

}  // namespace blink
