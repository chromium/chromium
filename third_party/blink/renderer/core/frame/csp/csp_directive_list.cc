// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"

#include <memory>
#include <utility>

#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/sandbox_flags.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

String GetSha256String(const String& content) {
  DigestValue digest;
  StringUTF8Adaptor utf8_content(content);
  bool digest_success = ComputeDigest(kHashAlgorithmSha256, utf8_content.data(),
                                      utf8_content.size(), digest);
  if (!digest_success) {
    return "sha256-...";
  }

  return "sha256-" + Base64Encode(digest);
}

ContentSecurityPolicyHashAlgorithm ConvertHashAlgorithmToCSPHashAlgorithm(
    IntegrityAlgorithm algorithm) {
  switch (algorithm) {
    case IntegrityAlgorithm::kSha256:
      return kContentSecurityPolicyHashAlgorithmSha256;
    case IntegrityAlgorithm::kSha384:
      return kContentSecurityPolicyHashAlgorithmSha384;
    case IntegrityAlgorithm::kSha512:
      return kContentSecurityPolicyHashAlgorithmSha512;
  }
  NOTREACHED();
  return kContentSecurityPolicyHashAlgorithmNone;
}

// IntegrityMetadata (from SRI) has base64-encoded digest values, but CSP uses
// binary format. This converts from the former to the latter.
bool ParseBase64Digest(String base64, DigestValue* hash) {
  Vector<char> hash_vector;
  // We accept base64url-encoded data here by normalizing it to base64.
  if (!Base64Decode(NormalizeToBase64(base64), hash_vector))
    return false;
  if (hash_vector.IsEmpty() || hash_vector.size() > kMaxDigestSize)
    return false;
  hash->Append(reinterpret_cast<uint8_t*>(hash_vector.data()),
               hash_vector.size());
  return true;
}

// https://w3c.github.io/webappsec-csp/#effective-directive-for-inline-check
// TODO(hiroshige): The following two methods are slightly different.
// Investigate the correct behavior and merge them.
ContentSecurityPolicy::DirectiveType
GetDirectiveTypeForAllowInlineFromInlineType(
    ContentSecurityPolicy::InlineType inline_type) {
  // 1. Switch on type: [spec text]
  switch (inline_type) {
    // "script":
    // "navigation":
    // 1. Return script-src-elem. [spec text]
    case ContentSecurityPolicy::InlineType::kScript:
    case ContentSecurityPolicy::InlineType::kNavigation:
      return ContentSecurityPolicy::DirectiveType::kScriptSrcElem;

    // "script attribute":
    // 1. Return script-src-attr. [spec text]
    case ContentSecurityPolicy::InlineType::kScriptAttribute:
      return ContentSecurityPolicy::DirectiveType::kScriptSrcAttr;

    // "style":
    // 1. Return style-src-elem. [spec text]
    case ContentSecurityPolicy::InlineType::kStyle:
      return ContentSecurityPolicy::DirectiveType::kStyleSrcElem;

    // "style attribute":
    // 1. Return style-src-attr. [spec text]
    case ContentSecurityPolicy::InlineType::kStyleAttribute:
      return ContentSecurityPolicy::DirectiveType::kStyleSrcAttr;
  }
}

ContentSecurityPolicy::DirectiveType GetDirectiveTypeForAllowHashFromInlineType(
    ContentSecurityPolicy::InlineType inline_type) {
  switch (inline_type) {
    case ContentSecurityPolicy::InlineType::kScript:
      return ContentSecurityPolicy::DirectiveType::kScriptSrcElem;

    case ContentSecurityPolicy::InlineType::kNavigation:
    case ContentSecurityPolicy::InlineType::kScriptAttribute:
      return ContentSecurityPolicy::DirectiveType::kScriptSrcAttr;

    case ContentSecurityPolicy::InlineType::kStyleAttribute:
      return ContentSecurityPolicy::DirectiveType::kStyleSrcAttr;

    case ContentSecurityPolicy::InlineType::kStyle:
      return ContentSecurityPolicy::DirectiveType::kStyleSrcElem;
  }
}

}  // namespace

using network::mojom::ContentSecurityPolicySource;
using network::mojom::ContentSecurityPolicyType;

CSPDirectiveList::CSPDirectiveList(ContentSecurityPolicy* policy,
                                   ContentSecurityPolicyType type,
                                   ContentSecurityPolicySource source)
    : policy_(policy),
      header_type_(type),
      header_source_(source),
      has_sandbox_policy_(false),
      strict_mixed_content_checking_enforced_(false),
      upgrade_insecure_requests_(false),
      use_reporting_api_(false) {}

CSPDirectiveList* CSPDirectiveList::Create(ContentSecurityPolicy* policy,
                                           const UChar* begin,
                                           const UChar* end,
                                           ContentSecurityPolicyType type,
                                           ContentSecurityPolicySource source,
                                           bool should_parse_wasm_eval) {
  CSPDirectiveList* directives =
      MakeGarbageCollected<CSPDirectiveList>(policy, type, source);
  directives->Parse(begin, end, should_parse_wasm_eval);

  if (!directives->CheckEval(directives->OperativeDirective(
          ContentSecurityPolicy::DirectiveType::kScriptSrc))) {
    String message =
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: \"" +
        directives
            ->OperativeDirective(
                ContentSecurityPolicy::DirectiveType::kScriptSrc)
            ->GetText() +
        "\".\n";
    directives->SetEvalDisabledErrorMessage(message);
  } else if (directives->RequiresTrustedTypes()) {
    String message =
        "Refused to evaluate a string as JavaScript because this document "
        "requires 'Trusted Type' assignment.";
    directives->SetEvalDisabledErrorMessage(message);
  }

  return directives;
}

void CSPDirectiveList::ReportViolation(
    const String& directive_text,
    const ContentSecurityPolicy::DirectiveType effective_type,
    const String& console_message,
    const KURL& blocked_url,
    ResourceRequest::RedirectStatus redirect_status,
    ContentSecurityPolicy::ContentSecurityPolicyViolationType violation_type,
    const String& sample,
    const String& sample_prefix) const {
  String message =
      IsReportOnly() ? "[Report Only] " + console_message : console_message;
  policy_->LogToConsole(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError, message));
  policy_->ReportViolation(directive_text, effective_type, message, blocked_url,
                           report_endpoints_, use_reporting_api_, header_,
                           header_type_, violation_type,
                           std::unique_ptr<SourceLocation>(),
                           nullptr,  // localFrame
                           redirect_status,
                           nullptr,  // Element*
                           sample, sample_prefix);
}

void CSPDirectiveList::ReportViolationWithFrame(
    const String& directive_text,
    const ContentSecurityPolicy::DirectiveType effective_type,
    const String& console_message,
    const KURL& blocked_url,
    LocalFrame* frame) const {
  String message =
      IsReportOnly() ? "[Report Only] " + console_message : console_message;
  policy_->LogToConsole(MakeGarbageCollected<ConsoleMessage>(
                            mojom::ConsoleMessageSource::kSecurity,
                            mojom::ConsoleMessageLevel::kError, message),
                        frame);
  policy_->ReportViolation(directive_text, effective_type, message, blocked_url,
                           report_endpoints_, use_reporting_api_, header_,
                           header_type_, ContentSecurityPolicy::kURLViolation,
                           std::unique_ptr<SourceLocation>(), frame);
}

void CSPDirectiveList::ReportViolationWithLocation(
    const String& directive_text,
    const ContentSecurityPolicy::DirectiveType effective_type,
    const String& console_message,
    const KURL& blocked_url,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    Element* element,
    const String& source) const {
  String message =
      IsReportOnly() ? "[Report Only] " + console_message : console_message;
  std::unique_ptr<SourceLocation> source_location =
      SourceLocation::Capture(context_url, context_line.OneBasedInt(), 0);
  policy_->LogToConsole(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError, message, source_location->Clone()));
  policy_->ReportViolation(directive_text, effective_type, message, blocked_url,
                           report_endpoints_, use_reporting_api_, header_,
                           header_type_,
                           ContentSecurityPolicy::kInlineViolation,
                           std::move(source_location), nullptr,  // localFrame
                           RedirectStatus::kNoRedirect, element, source);
}

void CSPDirectiveList::ReportEvalViolation(
    const String& directive_text,
    const ContentSecurityPolicy::DirectiveType effective_type,
    const String& message,
    const KURL& blocked_url,
    const ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  String report_message = IsReportOnly() ? "[Report Only] " + message : message;
  // Print a console message if it won't be redundant with a
  // JavaScript exception that the caller will throw. (Exceptions will
  // never get thrown in report-only mode because the caller won't see
  // a violation.)
  if (IsReportOnly() ||
      exception_status == ContentSecurityPolicy::kWillNotThrowException) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError, report_message);
    policy_->LogToConsole(console_message);
  }
  policy_->ReportViolation(directive_text, effective_type, message, blocked_url,
                           report_endpoints_, use_reporting_api_, header_,
                           header_type_, ContentSecurityPolicy::kEvalViolation,
                           std::unique_ptr<SourceLocation>(), nullptr,
                           RedirectStatus::kNoRedirect, nullptr, content);
}

bool CSPDirectiveList::CheckEval(SourceListDirective* directive) const {
  return !directive || directive->AllowEval();
}

bool CSPDirectiveList::CheckWasmEval(SourceListDirective* directive) const {
  return !directive || directive->AllowWasmEval();
}

bool CSPDirectiveList::IsMatchingNoncePresent(SourceListDirective* directive,
                                              const String& nonce) const {
  return directive && directive->AllowNonce(nonce);
}

bool CSPDirectiveList::AreAllMatchingHashesPresent(
    SourceListDirective* directive,
    const IntegrityMetadataSet& hashes) const {
  if (!directive || hashes.IsEmpty())
    return false;
  for (const std::pair<String, IntegrityAlgorithm>& hash : hashes) {
    // Convert the hash from integrity metadata format to CSP format.
    CSPHashValue csp_hash;
    csp_hash.first = ConvertHashAlgorithmToCSPHashAlgorithm(hash.second);
    if (!ParseBase64Digest(hash.first, &csp_hash.second))
      return false;
    // All integrity hashes must be listed in the CSP.
    if (!directive->AllowHash(csp_hash))
      return false;
  }
  return true;
}

bool CSPDirectiveList::CheckHash(SourceListDirective* directive,
                                 const CSPHashValue& hash_value) const {
  return !directive || directive->AllowHash(hash_value);
}

bool CSPDirectiveList::CheckUnsafeHashesAllowed(
    SourceListDirective* directive) const {
  return !directive || directive->AllowUnsafeHashes();
}

bool CSPDirectiveList::CheckDynamic(SourceListDirective* directive) const {
  return !directive || directive->AllowDynamic();
}

void CSPDirectiveList::ReportMixedContent(
    const KURL& blocked_url,
    ResourceRequest::RedirectStatus redirect_status) const {
  if (StrictMixedContentChecking()) {
    policy_->ReportViolation(
        ContentSecurityPolicy::GetDirectiveName(
            ContentSecurityPolicy::DirectiveType::kBlockAllMixedContent),
        ContentSecurityPolicy::DirectiveType::kBlockAllMixedContent, String(),
        blocked_url, report_endpoints_, use_reporting_api_, header_,
        header_type_, ContentSecurityPolicy::kURLViolation,
        std::unique_ptr<SourceLocation>(),
        nullptr,  // contextFrame,
        redirect_status);
  }
}

bool CSPDirectiveList::AllowTrustedTypeAssignmentFailure(
    const String& message,
    const String& sample,
    const String& sample_prefix) const {
  if (!require_trusted_types_for_ || !require_trusted_types_for_->require())
    return true;

  ReportViolation(
      ContentSecurityPolicy::GetDirectiveName(
          ContentSecurityPolicy::DirectiveType::kRequireTrustedTypesFor),
      ContentSecurityPolicy::DirectiveType::kRequireTrustedTypesFor, message,
      KURL(), RedirectStatus::kNoRedirect,
      ContentSecurityPolicy::kTrustedTypesSinkViolation, sample, sample_prefix);
  return IsReportOnly();
}

bool CSPDirectiveList::CheckSource(
    SourceListDirective* directive,
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status) const {
  // If |url| is empty, fall back to the policy URL to ensure that <object>'s
  // without a `src` can be blocked/allowed, as they can still load plugins
  // even though they don't actually have a URL.
  return !directive ||
         directive->Allows(
             url.IsEmpty() ? policy_->FallbackUrlForPlugin() : url,
             redirect_status);
}

bool CSPDirectiveList::CheckMediaType(MediaListDirective* directive,
                                      const String& type,
                                      const String& type_attribute) const {
  if (!directive)
    return true;
  if (type_attribute.IsEmpty() || type_attribute.StripWhiteSpace() != type)
    return false;
  return directive->Allows(type);
}

bool CSPDirectiveList::CheckEvalAndReportViolation(
    SourceListDirective* directive,
    const String& console_message,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  if (CheckEval(directive))
    return true;

  String suffix = String();
  if (directive == default_src_)
    suffix =
        " Note that 'script-src' was not explicitly set, so 'default-src' is "
        "used as a fallback.";

  ReportEvalViolation(
      directive->GetText(), ContentSecurityPolicy::DirectiveType::kScriptSrc,
      console_message + "\"" + directive->GetText() + "\"." + suffix + "\n",
      KURL(), exception_status,
      directive->AllowReportSample() ? content : g_empty_string);
  if (!IsReportOnly()) {
    policy_->ReportBlockedScriptExecutionToInspector(directive->GetText());
    return false;
  }
  return true;
}

bool CSPDirectiveList::CheckWasmEvalAndReportViolation(
    SourceListDirective* directive,
    const String& console_message,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  if (CheckWasmEval(directive))
    return true;

  String suffix = String();
  if (directive == default_src_) {
    suffix =
        " Note that 'script-src' was not explicitly set, so 'default-src' is "
        "used as a fallback.";
  }

  ReportEvalViolation(
      directive->GetText(), ContentSecurityPolicy::DirectiveType::kScriptSrc,
      console_message + "\"" + directive->GetText() + "\"." + suffix + "\n",
      KURL(), exception_status,
      directive->AllowReportSample() ? content : g_empty_string);
  if (!IsReportOnly()) {
    policy_->ReportBlockedScriptExecutionToInspector(directive->GetText());
    return false;
  }
  return true;
}

bool CSPDirectiveList::CheckMediaTypeAndReportViolation(
    MediaListDirective* directive,
    const String& type,
    const String& type_attribute,
    const String& console_message) const {
  if (CheckMediaType(directive, type, type_attribute))
    return true;

  String message = console_message + "\'" + directive->GetText() + "\'.";
  if (type_attribute.IsEmpty())
    message = message +
              " When enforcing the 'plugin-types' directive, the plugin's "
              "media type must be explicitly declared with a 'type' attribute "
              "on the containing element (e.g. '<object type=\"[TYPE GOES "
              "HERE]\" ...>').";

  // 'RedirectStatus::NoRedirect' is safe here, as we do the media type check
  // before actually loading data; this means that we shouldn't leak redirect
  // targets, as we won't have had a chance to redirect yet.
  ReportViolation(
      directive->GetText(), ContentSecurityPolicy::DirectiveType::kPluginTypes,
      message + "\n", NullURL(), ResourceRequest::RedirectStatus::kNoRedirect);
  return DenyIfEnforcingPolicy();
}

bool CSPDirectiveList::CheckInlineAndReportViolation(
    SourceListDirective* directive,
    const String& console_message,
    Element* element,
    const String& source,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    bool is_script,
    const String& hash_value,
    ContentSecurityPolicy::DirectiveType effective_type) const {
  if (!directive || directive->AllowAllInline())
    return true;

  String suffix = String();
  if (directive->AllowInline() && directive->IsHashOrNoncePresent()) {
    // If inline is allowed, but a hash or nonce is present, we ignore
    // 'unsafe-inline'. Throw a reasonable error.
    suffix =
        " Note that 'unsafe-inline' is ignored if either a hash or nonce value "
        "is present in the source list.";
  } else {
    suffix =
        " Either the 'unsafe-inline' keyword, a hash ('" + hash_value +
        "'), or a nonce ('nonce-...') is required to enable inline execution.";
    if (directive == default_src_)
      suffix = suffix + " Note also that '" +
               String(is_script ? "script" : "style") +
               "-src' was not explicitly set, so 'default-src' is used as a "
               "fallback.";
  }

  ReportViolationWithLocation(
      directive->GetText(), effective_type,
      console_message + "\"" + directive->GetText() + "\"." + suffix + "\n",
      KURL(), context_url, context_line, element,
      directive->AllowReportSample() ? source : g_empty_string);

  if (!IsReportOnly()) {
    if (is_script)
      policy_->ReportBlockedScriptExecutionToInspector(directive->GetText());
    return false;
  }
  return true;
}

bool CSPDirectiveList::CheckSourceAndReportViolation(
    SourceListDirective* directive,
    const KURL& url,
    const ContentSecurityPolicy::DirectiveType effective_type,
    const KURL& url_before_redirects,
    ResourceRequest::RedirectStatus redirect_status) const {
  if (!directive)
    return true;

  // We ignore URL-based allowlists if we're allowing dynamic script injection.
  if (CheckSource(directive, url, redirect_status) && !CheckDynamic(directive))
    return true;

  // We should never have a violation against `child-src` or `default-src`
  // directly; the effective directive should always be one of the explicit
  // fetch directives.
  DCHECK_NE(ContentSecurityPolicy::DirectiveType::kChildSrc, effective_type);
  DCHECK_NE(ContentSecurityPolicy::DirectiveType::kDefaultSrc, effective_type);

  String prefix = "Refused to ";
  if (ContentSecurityPolicy::DirectiveType::kBaseURI == effective_type)
    prefix = prefix + "set the document's base URI to '";
  else if (ContentSecurityPolicy::DirectiveType::kWorkerSrc == effective_type)
    prefix = prefix + "create a worker from '";
  else if (ContentSecurityPolicy::DirectiveType::kConnectSrc == effective_type)
    prefix = prefix + "connect to '";
  else if (ContentSecurityPolicy::DirectiveType::kFontSrc == effective_type)
    prefix = prefix + "load the font '";
  else if (ContentSecurityPolicy::DirectiveType::kFormAction == effective_type)
    prefix = prefix + "send form data to '";
  else if (ContentSecurityPolicy::DirectiveType::kFrameSrc == effective_type)
    prefix = prefix + "frame '";
  else if (ContentSecurityPolicy::DirectiveType::kImgSrc == effective_type)
    prefix = prefix + "load the image '";
  else if (ContentSecurityPolicy::DirectiveType::kMediaSrc == effective_type)
    prefix = prefix + "load media from '";
  else if (ContentSecurityPolicy::DirectiveType::kManifestSrc == effective_type)
    prefix = prefix + "load manifest from '";
  else if (ContentSecurityPolicy::DirectiveType::kObjectSrc == effective_type)
    prefix = prefix + "load plugin data from '";
  else if (ContentSecurityPolicy::DirectiveType::kPrefetchSrc == effective_type)
    prefix = prefix + "prefetch content from '";
  else if (ContentSecurityPolicy::IsScriptDirective(effective_type))
    prefix = prefix + "load the script '";
  else if (ContentSecurityPolicy::IsStyleDirective(effective_type))
    prefix = prefix + "load the stylesheet '";
  else if (ContentSecurityPolicy::DirectiveType::kNavigateTo == effective_type)
    prefix = prefix + "navigate to '";

  String suffix = String();
  if (CheckDynamic(directive)) {
    suffix =
        " 'strict-dynamic' is present, so host-based allowlisting is disabled.";
  }

  String directive_name = directive->GetName();
  String effective_directive_name =
      ContentSecurityPolicy::GetDirectiveName(effective_type);
  if (directive_name != effective_directive_name) {
    suffix = suffix + " Note that '" + effective_directive_name +
             "' was not explicitly set, so '" + directive_name +
             "' is used as a fallback.";
  }

  ReportViolation(directive->GetText(), effective_type,
                  prefix + url.ElidedString() +
                      "' because it violates the following Content Security "
                      "Policy directive: \"" +
                      directive->GetText() + "\"." + suffix + "\n",
                  url_before_redirects, redirect_status);
  return DenyIfEnforcingPolicy();
}

bool CSPDirectiveList::AllowInline(
    ContentSecurityPolicy::InlineType inline_type,
    Element* element,
    const String& content,
    const String& nonce,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    ReportingDisposition reporting_disposition) const {
  ContentSecurityPolicy::DirectiveType type =
      GetDirectiveTypeForAllowInlineFromInlineType(inline_type);

  SourceListDirective* directive = OperativeDirective(type);
  if (IsMatchingNoncePresent(directive, nonce))
    return true;

  auto* html_script_element = DynamicTo<HTMLScriptElement>(element);
  if (html_script_element &&
      inline_type == ContentSecurityPolicy::InlineType::kScript &&
      !html_script_element->Loader()->IsParserInserted() &&
      AllowDynamic(type)) {
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
        directive,
        "Refused to " + message +
            " because it violates the following Content Security Policy "
            "directive: ",
        element, content, context_url, context_line,
        ContentSecurityPolicy::IsScriptInlineType(inline_type), hash_value,
        type);
  }

  return !directive || directive->AllowAllInline();
}

bool CSPDirectiveList::ShouldCheckEval() const {
  return !CheckEval(
      OperativeDirective(ContentSecurityPolicy::DirectiveType::kScriptSrc));
}

bool CSPDirectiveList::AllowEval(
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  if (reporting_disposition == ReportingDisposition::kReport) {
    return CheckEvalAndReportViolation(
        OperativeDirective(ContentSecurityPolicy::DirectiveType::kScriptSrc),
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: ",
        exception_status, content);
  }
  return IsReportOnly() ||
         CheckEval(OperativeDirective(
             ContentSecurityPolicy::DirectiveType::kScriptSrc));
}

bool CSPDirectiveList::AllowWasmEval(
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  if (reporting_disposition == ReportingDisposition::kReport) {
    return CheckWasmEvalAndReportViolation(
        OperativeDirective(ContentSecurityPolicy::DirectiveType::kScriptSrc),
        "Refused to compile or instantiate WebAssembly module because "
        "'wasm-eval' is not an allowed source of script in the following "
        "Content Security Policy directive: ",
        exception_status, content);
  }
  return IsReportOnly() ||
         CheckWasmEval(OperativeDirective(
             ContentSecurityPolicy::DirectiveType::kScriptSrc));
}

bool CSPDirectiveList::ShouldDisableEvalBecauseScriptSrc() const {
  return !AllowEval(ReportingDisposition::kSuppressReporting,
                    ContentSecurityPolicy::kWillNotThrowException,
                    g_empty_string);
}

bool CSPDirectiveList::ShouldDisableEvalBecauseTrustedTypes() const {
  return RequiresTrustedTypes();
}

bool CSPDirectiveList::AllowPluginType(
    const String& type,
    const String& type_attribute,
    const KURL& url,
    ReportingDisposition reporting_disposition) const {
  return reporting_disposition == ReportingDisposition::kReport
             ? CheckMediaTypeAndReportViolation(
                   plugin_types_.Get(), type, type_attribute,
                   "Refused to load '" + url.ElidedString() + "' (MIME type '" +
                       type_attribute +
                       "') because it violates the following Content Security "
                       "Policy Directive: ")
             : CheckMediaType(plugin_types_.Get(), type, type_attribute);
}

bool CSPDirectiveList::AllowFromSource(
    ContentSecurityPolicy::DirectiveType type,
    const KURL& url,
    const KURL& url_before_redirects,
    ResourceRequest::RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    const String& nonce,
    const IntegrityMetadataSet& hashes,
    ParserDisposition parser_disposition) const {
  DCHECK(type == ContentSecurityPolicy::DirectiveType::kBaseURI ||
         type == ContentSecurityPolicy::DirectiveType::kConnectSrc ||
         type == ContentSecurityPolicy::DirectiveType::kFontSrc ||
         type == ContentSecurityPolicy::DirectiveType::kFormAction ||
         type == ContentSecurityPolicy::DirectiveType::kFrameSrc ||
         type == ContentSecurityPolicy::DirectiveType::kImgSrc ||
         type == ContentSecurityPolicy::DirectiveType::kManifestSrc ||
         type == ContentSecurityPolicy::DirectiveType::kMediaSrc ||
         type == ContentSecurityPolicy::DirectiveType::kObjectSrc ||
         type == ContentSecurityPolicy::DirectiveType::kPrefetchSrc ||
         type == ContentSecurityPolicy::DirectiveType::kScriptSrcElem ||
         type == ContentSecurityPolicy::DirectiveType::kStyleSrcElem ||
         type == ContentSecurityPolicy::DirectiveType::kWorkerSrc);

  if (type == ContentSecurityPolicy::DirectiveType::kObjectSrc ||
      type == ContentSecurityPolicy::DirectiveType::kFrameSrc) {
    if (url.ProtocolIsAbout())
      return true;
  }

  if (type == ContentSecurityPolicy::DirectiveType::kWorkerSrc &&
      AllowDynamicWorker())
    return true;

  if (type == ContentSecurityPolicy::DirectiveType::kScriptSrcElem ||
      type == ContentSecurityPolicy::DirectiveType::kStyleSrcElem) {
    if (IsMatchingNoncePresent(OperativeDirective(type), nonce))
      return true;
  }

  if (type == ContentSecurityPolicy::DirectiveType::kScriptSrcElem) {
    if (parser_disposition == kNotParserInserted && AllowDynamic(type))
      return true;
    if (AreAllMatchingHashesPresent(OperativeDirective(type), hashes))
      return true;
  }

  bool result =
      reporting_disposition == ReportingDisposition::kReport
          ? CheckSourceAndReportViolation(OperativeDirective(type), url, type,
                                          url_before_redirects, redirect_status)
          : CheckSource(OperativeDirective(type), url, redirect_status);

  if (type == ContentSecurityPolicy::DirectiveType::kBaseURI) {
    if (result &&
        !CheckSource(OperativeDirective(type), url, redirect_status)) {
      policy_->Count(WebFeature::kBaseWouldBeBlockedByDefaultSrc);
    }
  }

  return result;
}

bool CSPDirectiveList::AllowTrustedTypePolicy(const String& policy_name,
                                              bool is_duplicate) const {
  if (!trusted_types_ || trusted_types_->Allows(policy_name, is_duplicate))
    return true;

  ReportViolation(
      "trusted-types", ContentSecurityPolicy::DirectiveType::kTrustedTypes,
      String::Format(
          "Refused to create a TrustedTypePolicy named '%s' because "
          "it violates the following Content Security Policy directive: "
          "\"%s\".",
          policy_name.Utf8().c_str(),
          trusted_types_.Get()->GetText().Utf8().c_str()),
      KURL(), RedirectStatus::kNoRedirect,
      ContentSecurityPolicy::kTrustedTypesPolicyViolation, policy_name);

  return DenyIfEnforcingPolicy();
}

bool CSPDirectiveList::AllowHash(
    const CSPHashValue& hash_value,
    const ContentSecurityPolicy::InlineType inline_type) const {
  ContentSecurityPolicy::DirectiveType directive_type =
      GetDirectiveTypeForAllowHashFromInlineType(inline_type);

  // https://w3c.github.io/webappsec-csp/#match-element-to-source-list
  // Step 5. If type is "script" or "style", or unsafe-hashes flag is true:
  // [spec text]
  switch (inline_type) {
    case ContentSecurityPolicy::InlineType::kNavigation:
    case ContentSecurityPolicy::InlineType::kScriptAttribute:
    case ContentSecurityPolicy::InlineType::kStyleAttribute:
      if (!CheckUnsafeHashesAllowed(OperativeDirective(directive_type)))
        return false;
      break;

    case ContentSecurityPolicy::InlineType::kScript:
    case ContentSecurityPolicy::InlineType::kStyle:
      break;
  }
  return CheckHash(OperativeDirective(directive_type), hash_value);
}

bool CSPDirectiveList::AllowDynamic(
    ContentSecurityPolicy::DirectiveType directive_type) const {
  return CheckDynamic(OperativeDirective(directive_type));
}

bool CSPDirectiveList::AllowDynamicWorker() const {
  SourceListDirective* worker_src =
      OperativeDirective(ContentSecurityPolicy::DirectiveType::kWorkerSrc);
  return CheckDynamic(worker_src);
}

const String& CSPDirectiveList::PluginTypesText() const {
  DCHECK(HasPluginTypes());
  return plugin_types_->GetText();
}

bool CSPDirectiveList::ShouldSendCSPHeader(ResourceType type) const {
  // TODO(mkwst): Revisit this once the CORS prefetch issue with the 'CSP'
  //              header is worked out, one way or another:
  //              https://github.com/whatwg/fetch/issues/52
  return false;
}

// policy            = directive-list
// directive-list    = [ directive *( ";" [ directive ] ) ]
//
void CSPDirectiveList::Parse(const UChar* begin,
                             const UChar* end,
                             bool should_parse_wasm_eval) {
  header_ =
      String(begin, static_cast<wtf_size_t>(end - begin)).StripWhiteSpace();

  if (begin == end)
    return;

  const UChar* position = begin;
  while (position < end) {
    const UChar* directive_begin = position;
    SkipUntil<UChar>(position, end, ';');

    String name, value;
    if (ParseDirective(directive_begin, position, &name, &value)) {
      DCHECK(!name.IsEmpty());
      AddDirective(name, value);
    }

    DCHECK(position == end || *position == ';');
    SkipExactly<UChar>(position, end, ';');
  }
}

// directive         = *WSP [ directive-name [ WSP directive-value ] ]
// directive-name    = 1*( ALPHA / DIGIT / "-" )
// directive-value   = *( WSP / <VCHAR except ";"> )
//
bool CSPDirectiveList::ParseDirective(const UChar* begin,
                                      const UChar* end,
                                      String* name,
                                      String* value) {
  DCHECK(name->IsEmpty());
  DCHECK(value->IsEmpty());

  const UChar* position = begin;
  SkipWhile<UChar, IsASCIISpace>(position, end);

  // Empty directive (e.g. ";;;"). Exit early.
  if (position == end)
    return false;

  const UChar* name_begin = position;
  SkipWhile<UChar, IsCSPDirectiveNameCharacter>(position, end);

  // The directive-name must be non-empty.
  if (name_begin == position) {
    // Malformed CSP: directive starts with invalid characters
    policy_->Count(WebFeature::kMalformedCSP);

    SkipWhile<UChar, IsNotASCIISpace>(position, end);
    policy_->ReportUnsupportedDirective(
        String(name_begin, static_cast<wtf_size_t>(position - name_begin)));
    return false;
  }

  *name = String(name_begin, static_cast<wtf_size_t>(position - name_begin))
              .LowerASCII();

  if (position == end)
    return true;

  if (!SkipExactly<UChar, IsASCIISpace>(position, end)) {
    // Malformed CSP: after the directive name we don't have a space
    policy_->Count(WebFeature::kMalformedCSP);

    SkipWhile<UChar, IsNotASCIISpace>(position, end);
    policy_->ReportUnsupportedDirective(
        String(name_begin, static_cast<wtf_size_t>(position - name_begin)));
    return false;
  }

  SkipWhile<UChar, IsASCIISpace>(position, end);

  const UChar* value_begin = position;
  SkipWhile<UChar, IsCSPDirectiveValueCharacter>(position, end);

  if (position != end) {
    // Malformed CSP: directive value has invalid characters
    policy_->Count(WebFeature::kMalformedCSP);

    policy_->ReportInvalidDirectiveValueCharacter(
        *name, String(value_begin, static_cast<wtf_size_t>(end - value_begin)));
    return false;
  }

  // The directive-value may be empty.
  if (value_begin == position)
    return true;

  *value = String(value_begin, static_cast<wtf_size_t>(position - value_begin));
  return true;
}

void CSPDirectiveList::ParseReportTo(const String& name, const String& value) {
  if (!use_reporting_api_) {
    use_reporting_api_ = true;
    report_endpoints_.clear();
  }

  if (!report_endpoints_.IsEmpty()) {
    policy_->ReportDuplicateDirective(name);
    return;
  }

  ParseAndAppendReportEndpoints(value);

  if (report_endpoints_.size() > 1) {
    // The directive "report-to" only accepts one endpoint.
    report_endpoints_.Shrink(1);
    policy_->ReportMultipleReportToEndpoints();
  }
}

void CSPDirectiveList::ParseReportURI(const String& name, const String& value) {
  // report-to supersedes report-uri
  if (use_reporting_api_)
    return;

  if (!report_endpoints_.IsEmpty()) {
    policy_->ReportDuplicateDirective(name);
    return;
  }

  // Remove report-uri in meta policies, per
  // https://html.spec.whatwg.org/C/#attr-meta-http-equiv-content-security-policy.
  if (header_source_ == ContentSecurityPolicySource::kMeta) {
    policy_->ReportInvalidDirectiveInMeta(name);
    return;
  }

  ParseAndAppendReportEndpoints(value);

  // Ignore right away report-uri endpoints which would be blocked later when
  // reporting because of Mixed Content and report a warning.
  if (!policy_->GetSelfSource()) {
    return;
  }
  report_endpoints_.erase(
      std::remove_if(report_endpoints_.begin(), report_endpoints_.end(),
                     [this](const String& endpoint) {
                       KURL parsed_endpoint = KURL(endpoint);
                       if (!parsed_endpoint.IsValid()) {
                         // endpoint is not absolute, so it cannot violate
                         // MixedContent
                         return false;
                       }
                       if (MixedContentChecker::IsMixedContent(
                               policy_->GetSelfSource()->GetScheme(),
                               parsed_endpoint)) {
                         policy_->ReportMixedContentReportURI(endpoint);
                         return true;
                       }
                       return false;
                     }),
      report_endpoints_.end());
}

// For "report-uri" directive, this method corresponds to:
// https://w3c.github.io/webappsec-csp/#report-violation
// Step 3.4.2. For each token returned by splitting a string on ASCII whitespace
// with directive's value as the input. [spec text]

// For "report-to" directive, the spec says |value| is a single token
// but we use the same logic as "report-uri" and thus we split |value| by
// ASCII whitespaces. The tokens after the first one are discarded in
// CSPDirectiveList::ParseReportTo.
// https://w3c.github.io/webappsec-csp/#directive-report-to
void CSPDirectiveList::ParseAndAppendReportEndpoints(const String& value) {
  Vector<UChar> characters;
  value.AppendTo(characters);

  // https://infra.spec.whatwg.org/#split-on-ascii-whitespace

  // Step 2. Let tokens be a list of strings, initially empty. [spec text]
  DCHECK(report_endpoints_.IsEmpty());

  const UChar* position = characters.data();
  const UChar* end = position + characters.size();

  // Step 4. While position is not past the end of input: [spec text]
  while (position < end) {
    // Step 3. Skip ASCII whitespace within input given position. [spec text]
    // Step 4.3. Skip ASCII whitespace within input given position. [spec text]
    //
    // Note: IsASCIISpace returns true for U+000B which is not included in
    // https://infra.spec.whatwg.org/#ascii-whitespace.
    // TODO(mkwst): Investigate why the restrictions in the infra spec are
    // different than those in Blink here.
    SkipWhile<UChar, IsASCIISpace>(position, end);

    // Step 4.1. Let token be the result of collecting a sequence of code points
    // that are not ASCII whitespace from input, given position. [spec text]
    const UChar* endpoint_begin = position;
    SkipWhile<UChar, IsNotASCIISpace>(position, end);

    if (endpoint_begin < position) {
      // Step 4.2. Append token to tokens. [spec text]
      String endpoint = String(
          endpoint_begin, static_cast<wtf_size_t>(position - endpoint_begin));
      report_endpoints_.push_back(endpoint);
    }
  }

  policy_->Count(report_endpoints_.size() > 1
                     ? WebFeature::kReportUriMultipleEndpoints
                     : WebFeature::kReportUriSingleEndpoint);
}

template <class CSPDirectiveType>
void CSPDirectiveList::SetCSPDirective(const String& name,
                                       const String& value,
                                       Member<CSPDirectiveType>& directive,
                                       bool should_parse_wasm_eval) {
  if (directive) {
    policy_->ReportDuplicateDirective(name);
    return;
  }

  // Remove frame-ancestors directives in meta policies, per
  // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
  if (header_source_ == ContentSecurityPolicySource::kMeta &&
      ContentSecurityPolicy::GetDirectiveType(name) ==
          ContentSecurityPolicy::DirectiveType::kFrameAncestors) {
    policy_->ReportInvalidDirectiveInMeta(name);
    return;
  }

  directive = MakeGarbageCollected<CSPDirectiveType>(name, value, policy_);
}

void CSPDirectiveList::ApplySandboxPolicy(const String& name,
                                          const String& sandbox_policy) {
  // Remove sandbox directives in meta policies, per
  // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
  if (header_source_ == ContentSecurityPolicySource::kMeta) {
    policy_->ReportInvalidDirectiveInMeta(name);
    return;
  }
  if (IsReportOnly()) {
    policy_->ReportInvalidInReportOnly(name);
    return;
  }
  if (has_sandbox_policy_) {
    policy_->ReportDuplicateDirective(name);
    return;
  }

  using network::mojom::blink::WebSandboxFlags;
  WebSandboxFlags ignored_flags =
      !RuntimeEnabledFeatures::StorageAccessAPIEnabled()
          ? WebSandboxFlags::kStorageAccessByUserActivation
          : WebSandboxFlags::kNone;

  has_sandbox_policy_ = true;
  network::WebSandboxFlagsParsingResult parsed =
      network::ParseWebSandboxPolicy(sandbox_policy.Utf8(), ignored_flags);
  policy_->EnforceSandboxFlags(parsed.flags);
  if (!parsed.error_message.empty()) {
    policy_->ReportInvalidSandboxFlags(
        WebString::FromUTF8(parsed.error_message));
  }
}

void CSPDirectiveList::ApplyTreatAsPublicAddress() {
  // Remove treat-as-public-address directives in meta policies, per
  // https://wicg.github.io/cors-rfc1918/#csp
  if (header_source_ == ContentSecurityPolicySource::kMeta) {
    policy_->ReportInvalidDirectiveInMeta("treat-as-public-address");
    return;
  }

  // Remove treat-as-public-address directives in report-only, per
  // https://wicg.github.io/cors-rfc1918/#csp
  if (IsReportOnly()) {
    policy_->ReportInvalidInReportOnly("treat-as-public-address");
    return;
  }

  // Nothing to do, since treat-as-public-address directive is handled by the
  // browser process.
}

void CSPDirectiveList::AddTrustedTypes(const String& name,
                                       const String& value) {
  if (trusted_types_) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  trusted_types_ =
      MakeGarbageCollected<StringListDirective>(name, value, policy_);
}

void CSPDirectiveList::RequireTrustedTypesFor(const String& name,
                                              const String& value) {
  if (require_trusted_types_for_) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  require_trusted_types_for_ =
      MakeGarbageCollected<RequireTrustedTypesForDirective>(name, value,
                                                            policy_);
  if (require_trusted_types_for_->require())
    policy_->RequireTrustedTypes();
}

void CSPDirectiveList::EnforceStrictMixedContentChecking(const String& name,
                                                         const String& value) {
  if (strict_mixed_content_checking_enforced_) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  if (!value.IsEmpty())
    policy_->ReportValueForEmptyDirective(name, value);

  strict_mixed_content_checking_enforced_ = true;

  if (!IsReportOnly())
    policy_->EnforceStrictMixedContentChecking();
}

void CSPDirectiveList::EnableInsecureRequestsUpgrade(const String& name,
                                                     const String& value) {
  if (IsReportOnly()) {
    policy_->ReportInvalidInReportOnly(name);
    return;
  }
  if (upgrade_insecure_requests_) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  upgrade_insecure_requests_ = true;

  policy_->UpgradeInsecureRequests();
  if (!value.IsEmpty())
    policy_->ReportValueForEmptyDirective(name, value);
}

void CSPDirectiveList::AddDirective(const String& name, const String& value) {
  DCHECK(!name.IsEmpty());

  ContentSecurityPolicy::DirectiveType type =
      ContentSecurityPolicy::GetDirectiveType(name);
  switch (type) {
    case ContentSecurityPolicy::DirectiveType::kBaseURI:
      SetCSPDirective<SourceListDirective>(name, value, base_uri_);
      return;
    case ContentSecurityPolicy::DirectiveType::kBlockAllMixedContent:
      EnforceStrictMixedContentChecking(name, value);
      return;
    case ContentSecurityPolicy::DirectiveType::kChildSrc:
      SetCSPDirective<SourceListDirective>(name, value, child_src_);
      return;
    case ContentSecurityPolicy::DirectiveType::kConnectSrc:
      SetCSPDirective<SourceListDirective>(name, value, connect_src_);
      return;
    case ContentSecurityPolicy::DirectiveType::kDefaultSrc:
      SetCSPDirective<SourceListDirective>(name, value, default_src_);
      // TODO(mkwst) It seems unlikely that developers would use different
      // algorithms for scripts and styles. We may want to combine the
      // usesScriptHashAlgorithms() and usesStyleHashAlgorithms.
      policy_->UsesScriptHashAlgorithms(default_src_->HashAlgorithmsUsed());
      policy_->UsesStyleHashAlgorithms(default_src_->HashAlgorithmsUsed());
      return;
    case ContentSecurityPolicy::DirectiveType::kFontSrc:
      SetCSPDirective<SourceListDirective>(name, value, font_src_);
      return;
    case ContentSecurityPolicy::DirectiveType::kFormAction:
      SetCSPDirective<SourceListDirective>(name, value, form_action_);
      return;
    case ContentSecurityPolicy::DirectiveType::kFrameAncestors:
      SetCSPDirective<SourceListDirective>(name, value, frame_ancestors_);
      return;
    case ContentSecurityPolicy::DirectiveType::kFrameSrc:
      SetCSPDirective<SourceListDirective>(name, value, frame_src_);
      return;
    case ContentSecurityPolicy::DirectiveType::kImgSrc:
      SetCSPDirective<SourceListDirective>(name, value, img_src_);
      return;
    case ContentSecurityPolicy::DirectiveType::kManifestSrc:
      SetCSPDirective<SourceListDirective>(name, value, manifest_src_);
      return;
    case ContentSecurityPolicy::DirectiveType::kMediaSrc:
      SetCSPDirective<SourceListDirective>(name, value, media_src_);
      return;
    case ContentSecurityPolicy::DirectiveType::kNavigateTo:
      SetCSPDirective<SourceListDirective>(name, value, navigate_to_);
      return;
    case ContentSecurityPolicy::DirectiveType::kObjectSrc:
      SetCSPDirective<SourceListDirective>(name, value, object_src_);
      return;
    case ContentSecurityPolicy::DirectiveType::kPluginTypes:
      SetCSPDirective<MediaListDirective>(name, value, plugin_types_);
      return;
    case ContentSecurityPolicy::DirectiveType::kPrefetchSrc:
      if (!policy_->ExperimentalFeaturesEnabled())
        policy_->ReportUnsupportedDirective(name);
      else
        SetCSPDirective<SourceListDirective>(name, value, prefetch_src_);
      return;
    case ContentSecurityPolicy::DirectiveType::kReportTo:
      if (base::FeatureList::IsEnabled(network::features::kReporting))
        ParseReportTo(name, value);
      return;
    case ContentSecurityPolicy::DirectiveType::kReportURI:
      ParseReportURI(name, value);
      return;
    case ContentSecurityPolicy::DirectiveType::kRequireTrustedTypesFor:
      RequireTrustedTypesFor(name, value);
      return;
    case ContentSecurityPolicy::DirectiveType::kSandbox:
      ApplySandboxPolicy(name, value);
      return;
    case ContentSecurityPolicy::DirectiveType::kScriptSrc:
      SetCSPDirective<SourceListDirective>(name, value, script_src_);
      policy_->UsesScriptHashAlgorithms(script_src_->HashAlgorithmsUsed());
      return;
    case ContentSecurityPolicy::DirectiveType::kScriptSrcAttr:
      SetCSPDirective<SourceListDirective>(name, value, script_src_attr_);
      policy_->UsesScriptHashAlgorithms(script_src_attr_->HashAlgorithmsUsed());
      return;
    case ContentSecurityPolicy::DirectiveType::kScriptSrcElem:
      SetCSPDirective<SourceListDirective>(name, value, script_src_elem_);
      policy_->UsesScriptHashAlgorithms(script_src_elem_->HashAlgorithmsUsed());
      return;
    case ContentSecurityPolicy::DirectiveType::kStyleSrc:
      SetCSPDirective<SourceListDirective>(name, value, style_src_);
      policy_->UsesStyleHashAlgorithms(style_src_->HashAlgorithmsUsed());
      return;
    case ContentSecurityPolicy::DirectiveType::kStyleSrcAttr:
      SetCSPDirective<SourceListDirective>(name, value, style_src_attr_);
      policy_->UsesStyleHashAlgorithms(style_src_attr_->HashAlgorithmsUsed());
      return;
    case ContentSecurityPolicy::DirectiveType::kStyleSrcElem:
      SetCSPDirective<SourceListDirective>(name, value, style_src_elem_);
      policy_->UsesStyleHashAlgorithms(style_src_elem_->HashAlgorithmsUsed());
      return;
    case ContentSecurityPolicy::DirectiveType::kTreatAsPublicAddress:
      SetCSPDirective<SourceListDirective>(name, value, worker_src_);
      return;
    case ContentSecurityPolicy::DirectiveType::kTrustedTypes:
      AddTrustedTypes(name, value);
      return;
    case ContentSecurityPolicy::DirectiveType::kUpgradeInsecureRequests:
      EnableInsecureRequestsUpgrade(name, value);
      return;
    case ContentSecurityPolicy::DirectiveType::kUndefined:
      policy_->ReportUnsupportedDirective(name);
      return;
    case ContentSecurityPolicy::DirectiveType::kWorkerSrc:
      SetCSPDirective<SourceListDirective>(name, value, worker_src_);
      return;
  }
}

ContentSecurityPolicy::DirectiveType CSPDirectiveList::FallbackDirective(
    const ContentSecurityPolicy::DirectiveType current_directive,
    const ContentSecurityPolicy::DirectiveType original_directive) const {
  switch (current_directive) {
    case ContentSecurityPolicy::DirectiveType::kConnectSrc:
    case ContentSecurityPolicy::DirectiveType::kFontSrc:
    case ContentSecurityPolicy::DirectiveType::kImgSrc:
    case ContentSecurityPolicy::DirectiveType::kManifestSrc:
    case ContentSecurityPolicy::DirectiveType::kMediaSrc:
    case ContentSecurityPolicy::DirectiveType::kPrefetchSrc:
    case ContentSecurityPolicy::DirectiveType::kObjectSrc:
    case ContentSecurityPolicy::DirectiveType::kScriptSrc:
    case ContentSecurityPolicy::DirectiveType::kStyleSrc:
      return ContentSecurityPolicy::DirectiveType::kDefaultSrc;

    case ContentSecurityPolicy::DirectiveType::kScriptSrcAttr:
    case ContentSecurityPolicy::DirectiveType::kScriptSrcElem:
      return ContentSecurityPolicy::DirectiveType::kScriptSrc;

    case ContentSecurityPolicy::DirectiveType::kStyleSrcAttr:
    case ContentSecurityPolicy::DirectiveType::kStyleSrcElem:
      return ContentSecurityPolicy::DirectiveType::kStyleSrc;

    case ContentSecurityPolicy::DirectiveType::kFrameSrc:
    case ContentSecurityPolicy::DirectiveType::kWorkerSrc:
      return ContentSecurityPolicy::DirectiveType::kChildSrc;

    // Because the fallback chain of child-src can be different if we are
    // checking a worker or a frame request, we need to know the original type
    // of the request to decide. These are the fallback chains for worker-src
    // and frame-src specifically.

    // worker-src > child-src > script-src > default-src
    // frame-src > child-src > default-src

    // Since there are some situations and tests that will operate on the
    // `child-src` directive directly (like for example the EE subsumption
    // algorithm), we consider the child-src > default-src fallback path as the
    // "default" and the worker-src fallback path as an exception.
    case ContentSecurityPolicy::DirectiveType::kChildSrc:
      if (original_directive ==
          ContentSecurityPolicy::DirectiveType::kWorkerSrc)
        return ContentSecurityPolicy::DirectiveType::kScriptSrc;

      return ContentSecurityPolicy::DirectiveType::kDefaultSrc;

    default:
      return ContentSecurityPolicy::DirectiveType::kUndefined;
  }
}

SourceListDirective* CSPDirectiveList::OperativeDirective(
    const ContentSecurityPolicy::DirectiveType type,
    ContentSecurityPolicy::DirectiveType original_type) const {
  if (type == ContentSecurityPolicy::DirectiveType::kUndefined) {
    return nullptr;
  }

  SourceListDirective* directive;
  if (original_type == ContentSecurityPolicy::DirectiveType::kUndefined) {
    original_type = type;
  }

  switch (type) {
    case ContentSecurityPolicy::DirectiveType::kBaseURI:
      directive = base_uri_;
      break;
    case ContentSecurityPolicy::DirectiveType::kDefaultSrc:
      directive = default_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kFrameAncestors:
      directive = frame_ancestors_;
      break;
    case ContentSecurityPolicy::DirectiveType::kFormAction:
      directive = form_action_;
      break;
    case ContentSecurityPolicy::DirectiveType::kNavigateTo:
      directive = navigate_to_;
      break;
    case ContentSecurityPolicy::DirectiveType::kChildSrc:
      directive = child_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kConnectSrc:
      directive = connect_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kFontSrc:
      directive = font_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kImgSrc:
      directive = img_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kManifestSrc:
      directive = manifest_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kMediaSrc:
      directive = media_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kObjectSrc:
      directive = object_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kPrefetchSrc:
      directive = prefetch_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kScriptSrc:
      directive = script_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kScriptSrcAttr:
      directive = script_src_attr_;
      break;
    case ContentSecurityPolicy::DirectiveType::kScriptSrcElem:
      directive = script_src_elem_;
      break;
    case ContentSecurityPolicy::DirectiveType::kStyleSrc:
      directive = style_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kStyleSrcAttr:
      directive = style_src_attr_;
      break;
    case ContentSecurityPolicy::DirectiveType::kStyleSrcElem:
      directive = style_src_elem_;
      break;
    case ContentSecurityPolicy::DirectiveType::kFrameSrc:
      directive = frame_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kWorkerSrc:
      directive = worker_src_;
      break;
    default:
      return nullptr;
  }

  // if the directive does not exist, rely on the fallback directive
  return directive ? directive
                   : OperativeDirective(FallbackDirective(type, original_type),
                                        original_type);
}

SourceListDirectiveVector CSPDirectiveList::GetSourceVector(
    const ContentSecurityPolicy::DirectiveType type,
    const CSPDirectiveListVector& policies) {
  SourceListDirectiveVector source_list_directives;
  for (const auto& policy : policies) {
    if (SourceListDirective* directive = policy->OperativeDirective(type)) {
      if (directive->IsNone())
        return SourceListDirectiveVector(1, directive);
      source_list_directives.push_back(directive);
    }
  }

  return source_list_directives;
}

bool CSPDirectiveList::Subsumes(const CSPDirectiveListVector& other) {
  // A list of directives that we consider for subsumption.
  // See more about source lists here:
  // https://w3c.github.io/webappsec-csp/#framework-directive-source-list
  static ContentSecurityPolicy::DirectiveType directives[] = {
      ContentSecurityPolicy::DirectiveType::kChildSrc,
      ContentSecurityPolicy::DirectiveType::kConnectSrc,
      ContentSecurityPolicy::DirectiveType::kFontSrc,
      ContentSecurityPolicy::DirectiveType::kFrameSrc,
      ContentSecurityPolicy::DirectiveType::kImgSrc,
      ContentSecurityPolicy::DirectiveType::kManifestSrc,
      ContentSecurityPolicy::DirectiveType::kMediaSrc,
      ContentSecurityPolicy::DirectiveType::kObjectSrc,
      ContentSecurityPolicy::DirectiveType::kScriptSrc,
      ContentSecurityPolicy::DirectiveType::kScriptSrcAttr,
      ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
      ContentSecurityPolicy::DirectiveType::kStyleSrc,
      ContentSecurityPolicy::DirectiveType::kStyleSrcAttr,
      ContentSecurityPolicy::DirectiveType::kStyleSrcElem,
      ContentSecurityPolicy::DirectiveType::kWorkerSrc,
      ContentSecurityPolicy::DirectiveType::kBaseURI,
      ContentSecurityPolicy::DirectiveType::kFrameAncestors,
      ContentSecurityPolicy::DirectiveType::kFormAction,
      ContentSecurityPolicy::DirectiveType::kNavigateTo};

  for (const auto& directive : directives) {
    // There should only be one SourceListDirective for each directive in
    // Embedding-CSP.
    SourceListDirectiveVector required_list =
        GetSourceVector(directive, CSPDirectiveListVector(1, this));
    if (!required_list.size())
      continue;
    SourceListDirective* required = required_list[0];
    // Aggregate all serialized source lists of the returned CSP into a vector
    // based on a directive type, defaulting accordingly (for example, to
    // `default-src`).
    SourceListDirectiveVector returned = GetSourceVector(directive, other);
    // TODO(amalika): Add checks for plugin-types, sandbox, disown-opener,
    // navigation-to, worker-src.
    if (!required->Subsumes(returned))
      return false;
  }

  if (!HasPluginTypes())
    return true;

  HeapVector<Member<MediaListDirective>> plugin_types_other;
  for (const auto& policy : other) {
    if (policy->HasPluginTypes())
      plugin_types_other.push_back(policy->plugin_types_);
  }

  return plugin_types_->Subsumes(plugin_types_other);
}

network::mojom::blink::ContentSecurityPolicyPtr
CSPDirectiveList::ExposeForNavigationalChecks() const {
  using CSPDirectiveName = network::mojom::blink::CSPDirectiveName;

  auto policy = network::mojom::blink::ContentSecurityPolicy::New();

  policy->use_reporting_api = use_reporting_api_;
  policy->report_endpoints = report_endpoints_;
  policy->header = network::mojom::blink::ContentSecurityPolicyHeader::New(
      header_, header_type_, header_source_);

  if (child_src_) {
    policy->directives.Set(CSPDirectiveName::ChildSrc,
                           child_src_->ExposeForNavigationalChecks());
  }

  if (default_src_) {
    policy->directives.Set(CSPDirectiveName::DefaultSrc,
                           default_src_->ExposeForNavigationalChecks());
  }

  if (form_action_) {
    policy->directives.Set(CSPDirectiveName::FormAction,
                           form_action_->ExposeForNavigationalChecks());
  }

  if (frame_src_) {
    policy->directives.Set(CSPDirectiveName::FrameSrc,
                           frame_src_->ExposeForNavigationalChecks());
  }

  if (navigate_to_) {
    policy->directives.Set(CSPDirectiveName::NavigateTo,
                           navigate_to_->ExposeForNavigationalChecks());
  }

  policy->upgrade_insecure_requests = upgrade_insecure_requests_;

  return policy;
}

bool CSPDirectiveList::IsObjectRestrictionReasonable() const {
  SourceListDirective* object_src =
      OperativeDirective(ContentSecurityPolicy::DirectiveType::kObjectSrc);
  return object_src && object_src->IsNone();
}

bool CSPDirectiveList::IsBaseRestrictionReasonable() const {
  return base_uri_ && (base_uri_->IsNone() || base_uri_->IsSelf());
}

bool CSPDirectiveList::IsScriptRestrictionReasonable() const {
  SourceListDirective* script_src =
      OperativeDirective(ContentSecurityPolicy::DirectiveType::kScriptSrc);

  // If no `script-src` enforcement occurs, or it allows any and all inline
  // script, the restriction is not reasonable.
  if (!script_src || script_src->AllowAllInline())
    return false;

  if (script_src->IsNone())
    return true;

  // Policies containing `'strict-dynamic'` are reasonable, as that keyword
  // ensures that host-based expressions and `'unsafe-inline'` are ignored.
  return script_src->IsHashOrNoncePresent() &&
         (script_src->AllowDynamic() || !script_src->AllowsURLBasedMatching());
}

void CSPDirectiveList::Trace(Visitor* visitor) const {
  visitor->Trace(policy_);
  visitor->Trace(plugin_types_);
  visitor->Trace(base_uri_);
  visitor->Trace(child_src_);
  visitor->Trace(connect_src_);
  visitor->Trace(default_src_);
  visitor->Trace(font_src_);
  visitor->Trace(form_action_);
  visitor->Trace(frame_ancestors_);
  visitor->Trace(frame_src_);
  visitor->Trace(img_src_);
  visitor->Trace(media_src_);
  visitor->Trace(manifest_src_);
  visitor->Trace(object_src_);
  visitor->Trace(prefetch_src_);
  visitor->Trace(script_src_);
  visitor->Trace(script_src_attr_);
  visitor->Trace(script_src_elem_);
  visitor->Trace(style_src_);
  visitor->Trace(style_src_attr_);
  visitor->Trace(style_src_elem_);
  visitor->Trace(worker_src_);
  visitor->Trace(navigate_to_);
  visitor->Trace(trusted_types_);
  visitor->Trace(require_trusted_types_for_);
}

}  // namespace blink
