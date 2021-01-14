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

String GetRawDirectiveForMessage(
    const HashMap<CSPDirectiveName, String> raw_directives,
    CSPDirectiveName directive_name) {
  StringBuilder builder;
  builder.Append(ContentSecurityPolicy::GetDirectiveName(directive_name));
  builder.Append(" ");
  builder.Append(raw_directives.at(directive_name));
  return builder.ToString();
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
  NOTREACHED();
  return network::mojom::blink::CSPHashAlgorithm::None;
}

// IntegrityMetadata (from SRI) has base64-encoded digest values, but CSP uses
// binary format. This converts from the former to the latter.
bool ParseBase64Digest(String base64, Vector<uint8_t>& hash) {
  DCHECK(hash.IsEmpty());

  // We accept base64url-encoded data here by normalizing it to base64.
  Vector<char> out;
  if (!Base64Decode(NormalizeToBase64(base64), out))
    return false;
  if (out.IsEmpty() || out.size() > kMaxDigestSize)
    return false;
  for (char el : out)
    hash.push_back(el);
  return true;
}

// https://w3c.github.io/webappsec-csp/#effective-directive-for-inline-check
// TODO(hiroshige): The following two methods are slightly different.
// Investigate the correct behavior and merge them.
CSPDirectiveName GetDirectiveTypeForAllowInlineFromInlineType(
    ContentSecurityPolicy::InlineType inline_type) {
  // 1. Switch on type: [spec text]
  switch (inline_type) {
    // "script":
    // "navigation":
    // 1. Return script-src-elem. [spec text]
    case ContentSecurityPolicy::InlineType::kScript:
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

CSPDirectiveName GetDirectiveTypeForAllowHashFromInlineType(
    ContentSecurityPolicy::InlineType inline_type) {
  switch (inline_type) {
    case ContentSecurityPolicy::InlineType::kScript:
      return CSPDirectiveName::ScriptSrcElem;

    case ContentSecurityPolicy::InlineType::kNavigation:
    case ContentSecurityPolicy::InlineType::kScriptAttribute:
      return CSPDirectiveName::ScriptSrcAttr;

    case ContentSecurityPolicy::InlineType::kStyleAttribute:
      return CSPDirectiveName::StyleSrcAttr;

    case ContentSecurityPolicy::InlineType::kStyle:
      return CSPDirectiveName::StyleSrcElem;
  }
}

}  // namespace

using network::mojom::ContentSecurityPolicySource;
using network::mojom::ContentSecurityPolicyType;

CSPDirectiveList::CSPDirectiveList(ContentSecurityPolicy* policy)
    : policy_(policy),
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
  CSPDirectiveList* directives = MakeGarbageCollected<CSPDirectiveList>(policy);
  directives->header_ = network::mojom::blink::ContentSecurityPolicyHeader::New(
      String(begin, static_cast<wtf_size_t>(end - begin)).StripWhiteSpace(),
      type, source);

  directives->Parse(begin, end, should_parse_wasm_eval);

  CSPOperativeDirective directive =
      directives->OperativeDirective(CSPDirectiveName::ScriptSrc);

  if (!directives->CheckEval(directive.source_list)) {
    String message =
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: \"" +
        GetRawDirectiveForMessage(directives->raw_directives_, directive.type) +
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
    CSPDirectiveName effective_type,
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
                           report_endpoints_, use_reporting_api_,
                           header_->header_value, header_->type, violation_type,
                           std::unique_ptr<SourceLocation>(),
                           nullptr,  // localFrame
                           redirect_status,
                           nullptr,  // Element*
                           sample, sample_prefix);
}

void CSPDirectiveList::ReportViolationWithLocation(
    const String& directive_text,
    CSPDirectiveName effective_type,
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
                           report_endpoints_, use_reporting_api_,
                           header_->header_value, header_->type,
                           ContentSecurityPolicy::kInlineViolation,
                           std::move(source_location), nullptr,  // localFrame
                           RedirectStatus::kNoRedirect, element, source);
}

void CSPDirectiveList::ReportEvalViolation(
    const String& directive_text,
    CSPDirectiveName effective_type,
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
  policy_->ReportViolation(
      directive_text, effective_type, message, blocked_url, report_endpoints_,
      use_reporting_api_, header_->header_value, header_->type,
      ContentSecurityPolicy::kEvalViolation, std::unique_ptr<SourceLocation>(),
      nullptr, RedirectStatus::kNoRedirect, nullptr, content);
}

bool CSPDirectiveList::CheckEval(
    const network::mojom::blink::CSPSourceList* directive) const {
  return !directive || directive->allow_eval;
}

bool CSPDirectiveList::CheckWasmEval(
    const network::mojom::blink::CSPSourceList* directive) const {
  return !directive || directive->allow_wasm_eval;
}

bool CSPDirectiveList::IsMatchingNoncePresent(
    const network::mojom::blink::CSPSourceList* directive,
    const String& nonce) const {
  return directive && CSPSourceListAllowNonce(*directive, nonce);
}

bool CSPDirectiveList::AreAllMatchingHashesPresent(
    const network::mojom::blink::CSPSourceList* directive,
    const IntegrityMetadataSet& hashes) const {
  if (!directive || hashes.IsEmpty())
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

bool CSPDirectiveList::CheckHash(
    const network::mojom::blink::CSPSourceList* directive,
    const network::mojom::blink::CSPHashSource& hash_value) const {
  return !directive || CSPSourceListAllowHash(*directive, hash_value);
}

bool CSPDirectiveList::CheckUnsafeHashesAllowed(
    const network::mojom::blink::CSPSourceList* directive) const {
  return !directive || directive->allow_unsafe_hashes;
}

bool CSPDirectiveList::CheckDynamic(
    const network::mojom::blink::CSPSourceList* directive,
    CSPDirectiveName effective_type) const {
  // 'strict-dynamic' only applies to scripts
  if (effective_type != CSPDirectiveName::ScriptSrc &&
      effective_type != CSPDirectiveName::ScriptSrcAttr &&
      effective_type != CSPDirectiveName::ScriptSrcElem &&
      effective_type != CSPDirectiveName::WorkerSrc) {
    return false;
  }
  return !directive || directive->allow_dynamic;
}

void CSPDirectiveList::ReportMixedContent(
    const KURL& blocked_url,
    ResourceRequest::RedirectStatus redirect_status) const {
  if (StrictMixedContentChecking()) {
    policy_->ReportViolation(ContentSecurityPolicy::GetDirectiveName(
                                 CSPDirectiveName::BlockAllMixedContent),
                             CSPDirectiveName::BlockAllMixedContent, String(),
                             blocked_url, report_endpoints_, use_reporting_api_,
                             header_->header_value, header_->type,
                             ContentSecurityPolicy::kURLViolation,
                             std::unique_ptr<SourceLocation>(),
                             nullptr,  // contextFrame,
                             redirect_status);
  }
}

bool CSPDirectiveList::RequiresTrustedTypes() const {
  return require_trusted_types_for_ ==
         network::mojom::blink::CSPRequireTrustedTypesFor::Script;
}

bool CSPDirectiveList::AllowTrustedTypeAssignmentFailure(
    const String& message,
    const String& sample,
    const String& sample_prefix) const {
  if (!RequiresTrustedTypes())
    return true;

  ReportViolation(ContentSecurityPolicy::GetDirectiveName(
                      CSPDirectiveName::RequireTrustedTypesFor),
                  CSPDirectiveName::RequireTrustedTypesFor, message, KURL(),
                  RedirectStatus::kNoRedirect,
                  ContentSecurityPolicy::kTrustedTypesSinkViolation, sample,
                  sample_prefix);
  return IsReportOnly();
}

bool CSPDirectiveList::CheckSource(
    const network::mojom::blink::CSPSourceList* directive,
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status) const {
  // If |url| is empty, fall back to the policy URL to ensure that <object>'s
  // without a `src` can be blocked/allowed, as they can still load plugins
  // even though they don't actually have a URL.
  if (!directive)
    return true;

  return CSPSourceListAllows(
      *directive, *(policy_->GetSelfSource()),
      url.IsEmpty() ? policy_->FallbackUrlForPlugin() : url, redirect_status);
}

bool CSPDirectiveList::CheckMediaType(const Vector<String>& plugin_types,
                                      const String& type,
                                      const String& type_attribute) const {
  if (type_attribute.IsEmpty() || type_attribute.StripWhiteSpace() != type)
    return false;
  return plugin_types.Contains(type);
}

bool CSPDirectiveList::CheckEvalAndReportViolation(
    const String& console_message,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  CSPOperativeDirective directive =
      OperativeDirective(CSPDirectiveName::ScriptSrc);
  if (CheckEval(directive.source_list))
    return true;

  String suffix = String();
  if (directive.type == CSPDirectiveName::DefaultSrc) {
    suffix =
        " Note that 'script-src' was not explicitly set, so 'default-src' is "
        "used as a fallback.";
  }

  String raw_directive =
      GetRawDirectiveForMessage(raw_directives_, directive.type);
  ReportEvalViolation(
      raw_directive, CSPDirectiveName::ScriptSrc,
      console_message + "\"" + raw_directive + "\"." + suffix + "\n", KURL(),
      exception_status,
      directive.source_list->report_sample ? content : g_empty_string);
  if (!IsReportOnly()) {
    policy_->ReportBlockedScriptExecutionToInspector(raw_directive);
    return false;
  }
  return true;
}

bool CSPDirectiveList::CheckWasmEvalAndReportViolation(
    const String& console_message,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  CSPOperativeDirective directive =
      OperativeDirective(CSPDirectiveName::ScriptSrc);
  if (CheckWasmEval(directive.source_list))
    return true;

  String suffix = String();
  if (directive.type == CSPDirectiveName::DefaultSrc) {
    suffix =
        " Note that 'script-src' was not explicitly set, so 'default-src' is "
        "used as a fallback.";
  }

  String raw_directive =
      GetRawDirectiveForMessage(raw_directives_, directive.type);
  ReportEvalViolation(
      raw_directive, CSPDirectiveName::ScriptSrc,
      console_message + "\"" + raw_directive + "\"." + suffix + "\n", KURL(),
      exception_status,
      directive.source_list->report_sample ? content : g_empty_string);
  if (!IsReportOnly()) {
    policy_->ReportBlockedScriptExecutionToInspector(raw_directive);
    return false;
  }
  return true;
}

bool CSPDirectiveList::CheckMediaTypeAndReportViolation(
    const Vector<String>& plugin_types,
    const String& type,
    const String& type_attribute,
    const String& console_message) const {
  if (CheckMediaType(plugin_types, type, type_attribute))
    return true;

  String raw_directive = GetRawDirectiveForMessage(
      raw_directives_, network::mojom::blink::CSPDirectiveName::PluginTypes);
  String message = console_message + "\'" + raw_directive + "\'.";
  if (type_attribute.IsEmpty())
    message = message +
              " When enforcing the 'plugin-types' directive, the plugin's "
              "media type must be explicitly declared with a 'type' attribute "
              "on the containing element (e.g. '<object type=\"[TYPE GOES "
              "HERE]\" ...>').";

  // 'RedirectStatus::NoRedirect' is safe here, as we do the media type check
  // before actually loading data; this means that we shouldn't leak redirect
  // targets, as we won't have had a chance to redirect yet.
  ReportViolation(raw_directive, CSPDirectiveName::PluginTypes, message + "\n",
                  NullURL(), ResourceRequest::RedirectStatus::kNoRedirect);
  return DenyIfEnforcingPolicy();
}

bool CSPDirectiveList::CheckInlineAndReportViolation(
    CSPOperativeDirective directive,
    const String& console_message,
    Element* element,
    const String& source,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    bool is_script,
    const String& hash_value,
    CSPDirectiveName effective_type) const {
  if (!directive.source_list ||
      CSPSourceListAllowAllInline(directive.type, *directive.source_list))
    return true;

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
    if (directive.type == CSPDirectiveName::DefaultSrc)
      suffix = suffix + " Note also that '" +
               String(is_script ? "script" : "style") +
               "-src' was not explicitly set, so 'default-src' is used as a "
               "fallback.";
  }

  String raw_directive =
      GetRawDirectiveForMessage(raw_directives_, directive.type);
  ReportViolationWithLocation(
      raw_directive, effective_type,
      console_message + "\"" + raw_directive + "\"." + suffix + "\n", KURL(),
      context_url, context_line, element,
      directive.source_list->report_sample ? source : g_empty_string);

  if (!IsReportOnly()) {
    if (is_script)
      policy_->ReportBlockedScriptExecutionToInspector(raw_directive);
    return false;
  }
  return true;
}

bool CSPDirectiveList::CheckSourceAndReportViolation(
    CSPOperativeDirective directive,
    const KURL& url,
    CSPDirectiveName effective_type,
    const KURL& url_before_redirects,
    ResourceRequest::RedirectStatus redirect_status) const {
  if (!directive.source_list)
    return true;

  // We ignore URL-based allowlists if we're allowing dynamic script injection.
  if (CheckSource(directive.source_list, url, redirect_status) &&
      !CheckDynamic(directive.source_list, effective_type))
    return true;

  // We should never have a violation against `child-src` or `default-src`
  // directly; the effective directive should always be one of the explicit
  // fetch directives.
  DCHECK_NE(CSPDirectiveName::ChildSrc, effective_type);
  DCHECK_NE(CSPDirectiveName::DefaultSrc, effective_type);

  String prefix = "Refused to ";
  if (CSPDirectiveName::BaseURI == effective_type)
    prefix = prefix + "set the document's base URI to '";
  else if (CSPDirectiveName::WorkerSrc == effective_type)
    prefix = prefix + "create a worker from '";
  else if (CSPDirectiveName::ConnectSrc == effective_type)
    prefix = prefix + "connect to '";
  else if (CSPDirectiveName::FontSrc == effective_type)
    prefix = prefix + "load the font '";
  else if (CSPDirectiveName::FormAction == effective_type)
    prefix = prefix + "send form data to '";
  else if (CSPDirectiveName::FrameSrc == effective_type)
    prefix = prefix + "frame '";
  else if (CSPDirectiveName::ImgSrc == effective_type)
    prefix = prefix + "load the image '";
  else if (CSPDirectiveName::MediaSrc == effective_type)
    prefix = prefix + "load media from '";
  else if (CSPDirectiveName::ManifestSrc == effective_type)
    prefix = prefix + "load manifest from '";
  else if (CSPDirectiveName::ObjectSrc == effective_type)
    prefix = prefix + "load plugin data from '";
  else if (CSPDirectiveName::PrefetchSrc == effective_type)
    prefix = prefix + "prefetch content from '";
  else if (ContentSecurityPolicy::IsScriptDirective(effective_type))
    prefix = prefix + "load the script '";
  else if (ContentSecurityPolicy::IsStyleDirective(effective_type))
    prefix = prefix + "load the stylesheet '";
  else if (CSPDirectiveName::NavigateTo == effective_type)
    prefix = prefix + "navigate to '";

  String suffix = String();
  if (CheckDynamic(directive.source_list, effective_type)) {
    suffix =
        " 'strict-dynamic' is present, so host-based allowlisting is disabled.";
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

  String raw_directive =
      GetRawDirectiveForMessage(raw_directives_, directive.type);
  ReportViolation(raw_directive, effective_type,
                  prefix + url.ElidedString() +
                      "' because it violates the following Content Security "
                      "Policy directive: \"" +
                      raw_directive + "\"." + suffix + "\n",
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
  CSPDirectiveName type =
      GetDirectiveTypeForAllowInlineFromInlineType(inline_type);

  CSPOperativeDirective directive = OperativeDirective(type);
  if (IsMatchingNoncePresent(directive.source_list, nonce))
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

  return !directive.source_list ||
         CSPSourceListAllowAllInline(directive.type, *directive.source_list);
}

bool CSPDirectiveList::ShouldCheckEval() const {
  return !CheckEval(
      OperativeDirective(CSPDirectiveName::ScriptSrc).source_list);
}

bool CSPDirectiveList::AllowEval(
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  if (reporting_disposition == ReportingDisposition::kReport) {
    return CheckEvalAndReportViolation(
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: ",
        exception_status, content);
  }
  return IsReportOnly() ||
         CheckEval(OperativeDirective(CSPDirectiveName::ScriptSrc).source_list);
}

bool CSPDirectiveList::AllowWasmEval(
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  if (reporting_disposition == ReportingDisposition::kReport) {
    return CheckWasmEvalAndReportViolation(
        "Refused to compile or instantiate WebAssembly module because "
        "'wasm-eval' is not an allowed source of script in the following "
        "Content Security Policy directive: ",
        exception_status, content);
  }
  return IsReportOnly() ||
         CheckWasmEval(
             OperativeDirective(CSPDirectiveName::ScriptSrc).source_list);
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
  if (!plugin_types_.has_value())
    return true;

  return reporting_disposition == ReportingDisposition::kReport
             ? CheckMediaTypeAndReportViolation(
                   plugin_types_.value(), type, type_attribute,
                   "Refused to load '" + url.ElidedString() + "' (MIME type '" +
                       type_attribute +
                       "') because it violates the following Content Security "
                       "Policy Directive: ")
             : CheckMediaType(plugin_types_.value(), type, type_attribute);
}

bool CSPDirectiveList::AllowFromSource(
    CSPDirectiveName type,
    const KURL& url,
    const KURL& url_before_redirects,
    ResourceRequest::RedirectStatus redirect_status,
    ReportingDisposition reporting_disposition,
    const String& nonce,
    const IntegrityMetadataSet& hashes,
    ParserDisposition parser_disposition) const {
  DCHECK(type == CSPDirectiveName::BaseURI ||
         type == CSPDirectiveName::ConnectSrc ||
         type == CSPDirectiveName::FontSrc ||
         type == CSPDirectiveName::FormAction ||
         type == CSPDirectiveName::FrameSrc ||
         type == CSPDirectiveName::ImgSrc ||
         type == CSPDirectiveName::ManifestSrc ||
         type == CSPDirectiveName::MediaSrc ||
         type == CSPDirectiveName::ObjectSrc ||
         type == CSPDirectiveName::PrefetchSrc ||
         type == CSPDirectiveName::ScriptSrcElem ||
         type == CSPDirectiveName::StyleSrcElem ||
         type == CSPDirectiveName::WorkerSrc);

  if (type == CSPDirectiveName::ObjectSrc ||
      type == CSPDirectiveName::FrameSrc) {
    if (url.ProtocolIsAbout())
      return true;
  }

  if (type == CSPDirectiveName::WorkerSrc && AllowDynamicWorker())
    return true;

  if (type == CSPDirectiveName::ScriptSrcElem ||
      type == CSPDirectiveName::StyleSrcElem) {
    if (IsMatchingNoncePresent(OperativeDirective(type).source_list, nonce))
      return true;
  }

  if (type == CSPDirectiveName::ScriptSrcElem) {
    if (parser_disposition == kNotParserInserted && AllowDynamic(type))
      return true;
    if (AreAllMatchingHashesPresent(OperativeDirective(type).source_list,
                                    hashes))
      return true;
  }

  CSPOperativeDirective directive = OperativeDirective(type);
  bool result =
      reporting_disposition == ReportingDisposition::kReport
          ? CheckSourceAndReportViolation(directive, url, type,
                                          url_before_redirects, redirect_status)
          : CheckSource(directive.source_list, url, redirect_status);

  if (type == CSPDirectiveName::BaseURI) {
    if (result && !CheckSource(directive.source_list, url, redirect_status)) {
      policy_->Count(WebFeature::kBaseWouldBeBlockedByDefaultSrc);
    }
  }

  return result;
}

bool CSPDirectiveList::AllowTrustedTypePolicy(
    const String& policy_name,
    bool is_duplicate,
    ContentSecurityPolicy::AllowTrustedTypePolicyDetails& violation_details)
    const {
  if (!trusted_types_ ||
      CSPTrustedTypesAllows(*trusted_types_, policy_name, is_duplicate,
                            violation_details)) {
    return true;
  }

  String raw_directive = GetRawDirectiveForMessage(
      raw_directives_, network::mojom::blink::CSPDirectiveName::TrustedTypes);
  ReportViolation(
      "trusted-types", CSPDirectiveName::TrustedTypes,
      String::Format(
          "Refused to create a TrustedTypePolicy named '%s' because "
          "it violates the following Content Security Policy directive: "
          "\"%s\".",
          policy_name.Utf8().c_str(), raw_directive.Utf8().c_str()),
      KURL(), RedirectStatus::kNoRedirect,
      ContentSecurityPolicy::kTrustedTypesPolicyViolation, policy_name);

  return DenyIfEnforcingPolicy();
}

bool CSPDirectiveList::AllowHash(
    const network::mojom::blink::CSPHashSource& hash_value,
    const ContentSecurityPolicy::InlineType inline_type) const {
  CSPDirectiveName directive_type =
      GetDirectiveTypeForAllowHashFromInlineType(inline_type);

  // https://w3c.github.io/webappsec-csp/#match-element-to-source-list
  // Step 5. If type is "script" or "style", or unsafe-hashes flag is true:
  // [spec text]
  switch (inline_type) {
    case ContentSecurityPolicy::InlineType::kNavigation:
    case ContentSecurityPolicy::InlineType::kScriptAttribute:
    case ContentSecurityPolicy::InlineType::kStyleAttribute:
      if (!CheckUnsafeHashesAllowed(
              OperativeDirective(directive_type).source_list))
        return false;
      break;

    case ContentSecurityPolicy::InlineType::kScript:
    case ContentSecurityPolicy::InlineType::kStyle:
      break;
  }
  return CheckHash(OperativeDirective(directive_type).source_list, hash_value);
}

bool CSPDirectiveList::AllowDynamic(CSPDirectiveName directive_type) const {
  return CheckDynamic(OperativeDirective(directive_type).source_list,
                      directive_type);
}

bool CSPDirectiveList::AllowDynamicWorker() const {
  const network::mojom::blink::CSPSourceList* worker_src =
      OperativeDirective(CSPDirectiveName::WorkerSrc).source_list;
  return CheckDynamic(worker_src, CSPDirectiveName::WorkerSrc);
}

String CSPDirectiveList::PluginTypesText() const {
  DCHECK(HasPluginTypes());
  return GetRawDirectiveForMessage(
      raw_directives_, network::mojom::blink::CSPDirectiveName::PluginTypes);
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
  if (header_->source == ContentSecurityPolicySource::kMeta) {
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
                               policy_->GetSelfSource()->scheme,
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

void CSPDirectiveList::ApplySandboxPolicy(const String& name,
                                          const String& sandbox_policy) {
  // Remove sandbox directives in meta policies, per
  // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
  if (header_->source == ContentSecurityPolicySource::kMeta) {
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
  if (header_->source == ContentSecurityPolicySource::kMeta) {
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

  CSPDirectiveName type = ContentSecurityPolicy::GetDirectiveType(name);

  if (type == CSPDirectiveName::Unknown) {
    policy_->ReportUnsupportedDirective(name);
    return;
  }

  if (!raw_directives_.insert(type, value).is_new_entry) {
    policy_->ReportDuplicateDirective(name);
    return;
  }

  network::mojom::blink::CSPSourceListPtr source_list = nullptr;

  switch (type) {
    case CSPDirectiveName::BaseURI:
      directives_.insert(type, CSPSourceListParse(name, value, policy_));
      return;
    case CSPDirectiveName::BlockAllMixedContent:
      EnforceStrictMixedContentChecking(name, value);
      return;
    case CSPDirectiveName::ChildSrc:
    case CSPDirectiveName::ConnectSrc:
      directives_.insert(type, CSPSourceListParse(name, value, policy_));
      return;
    case CSPDirectiveName::DefaultSrc:
      source_list = CSPSourceListParse(name, value, policy_);
      // TODO(mkwst) It seems unlikely that developers would use different
      // algorithms for scripts and styles. We may want to combine the
      // usesScriptHashAlgorithms() and usesStyleHashAlgorithms.
      policy_->UsesScriptHashAlgorithms(HashAlgorithmsUsed(source_list.get()));
      policy_->UsesStyleHashAlgorithms(HashAlgorithmsUsed(source_list.get()));
      directives_.insert(type, std::move(source_list));
      return;
    case CSPDirectiveName::FontSrc:
    case CSPDirectiveName::FormAction:
      directives_.insert(type, CSPSourceListParse(name, value, policy_));
      return;
    case CSPDirectiveName::FrameAncestors:
      // Remove frame-ancestors directives in meta policies, per
      // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
      if (header_->source == ContentSecurityPolicySource::kMeta) {
        policy_->ReportInvalidDirectiveInMeta(name);
      } else {
        directives_.insert(type, CSPSourceListParse(name, value, policy_));
      }
      return;
    case CSPDirectiveName::FrameSrc:
    case CSPDirectiveName::ImgSrc:
    case CSPDirectiveName::ManifestSrc:
    case CSPDirectiveName::MediaSrc:
    case CSPDirectiveName::NavigateTo:
    case CSPDirectiveName::ObjectSrc:
      directives_.insert(type, CSPSourceListParse(name, value, policy_));
      return;
    case CSPDirectiveName::PluginTypes:
      plugin_types_ = CSPPluginTypesParse(value, policy_);
      return;
    case CSPDirectiveName::PrefetchSrc:
      if (!policy_->ExperimentalFeaturesEnabled())
        policy_->ReportUnsupportedDirective(name);
      else
        directives_.insert(type, CSPSourceListParse(name, value, policy_));
      return;
    case CSPDirectiveName::ReportTo:
      if (base::FeatureList::IsEnabled(network::features::kReporting))
        ParseReportTo(name, value);
      return;
    case CSPDirectiveName::ReportURI:
      ParseReportURI(name, value);
      return;
    case CSPDirectiveName::RequireTrustedTypesFor:
      require_trusted_types_for_ =
          CSPRequireTrustedTypesForParse(value, policy_);
      if (RequiresTrustedTypes())
        policy_->RequireTrustedTypes();
      return;
    case CSPDirectiveName::Sandbox:
      ApplySandboxPolicy(name, value);
      return;
    case CSPDirectiveName::ScriptSrc:
    case CSPDirectiveName::ScriptSrcAttr:
    case CSPDirectiveName::ScriptSrcElem:
      source_list = CSPSourceListParse(name, value, policy_);
      policy_->UsesScriptHashAlgorithms(HashAlgorithmsUsed(source_list.get()));
      directives_.insert(type, std::move(source_list));
      return;
    case CSPDirectiveName::StyleSrc:
    case CSPDirectiveName::StyleSrcAttr:
    case CSPDirectiveName::StyleSrcElem:
      source_list = CSPSourceListParse(name, value, policy_);
      policy_->UsesStyleHashAlgorithms(HashAlgorithmsUsed(source_list.get()));
      directives_.insert(type, std::move(source_list));
      return;
    case CSPDirectiveName::TreatAsPublicAddress:
      ApplyTreatAsPublicAddress();
      return;
    case CSPDirectiveName::TrustedTypes:
      trusted_types_ = CSPTrustedTypesParse(value, policy_);
      return;
    case CSPDirectiveName::UpgradeInsecureRequests:
      EnableInsecureRequestsUpgrade(name, value);
      return;
    case CSPDirectiveName::Unknown:
      NOTREACHED();
      return;
    case CSPDirectiveName::WorkerSrc:
      directives_.insert(type, CSPSourceListParse(name, value, policy_));
      return;
  }
}

CSPDirectiveName CSPDirectiveList::FallbackDirective(
    CSPDirectiveName current_directive,
    CSPDirectiveName original_directive) const {
  switch (current_directive) {
    case CSPDirectiveName::ConnectSrc:
    case CSPDirectiveName::FontSrc:
    case CSPDirectiveName::ImgSrc:
    case CSPDirectiveName::ManifestSrc:
    case CSPDirectiveName::MediaSrc:
    case CSPDirectiveName::PrefetchSrc:
    case CSPDirectiveName::ObjectSrc:
    case CSPDirectiveName::ScriptSrc:
    case CSPDirectiveName::StyleSrc:
      return CSPDirectiveName::DefaultSrc;

    case CSPDirectiveName::ScriptSrcAttr:
    case CSPDirectiveName::ScriptSrcElem:
      return CSPDirectiveName::ScriptSrc;

    case CSPDirectiveName::StyleSrcAttr:
    case CSPDirectiveName::StyleSrcElem:
      return CSPDirectiveName::StyleSrc;

    case CSPDirectiveName::FrameSrc:
    case CSPDirectiveName::WorkerSrc:
      return CSPDirectiveName::ChildSrc;

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
    case CSPDirectiveName::ChildSrc:
      if (original_directive == CSPDirectiveName::WorkerSrc)
        return CSPDirectiveName::ScriptSrc;

      return CSPDirectiveName::DefaultSrc;

    default:
      return CSPDirectiveName::Unknown;
  }
}

CSPOperativeDirective CSPDirectiveList::OperativeDirective(
    CSPDirectiveName type,
    CSPDirectiveName original_type) const {
  if (type == CSPDirectiveName::Unknown) {
    return CSPOperativeDirective{CSPDirectiveName::Unknown, nullptr};
  }

  if (original_type == CSPDirectiveName::Unknown) {
    original_type = type;
  }

  const auto directive = directives_.find(type);

  // if the directive does not exist, rely on the fallback directive
  return (directive != directives_.end())
             ? CSPOperativeDirective{type, directive->value.get()}
             : OperativeDirective(FallbackDirective(type, original_type),
                                  original_type);
}

network::mojom::blink::ContentSecurityPolicyPtr
CSPDirectiveList::ExposeForNavigationalChecks() const {
  auto policy = network::mojom::blink::ContentSecurityPolicy::New();

  policy->self_origin =
      policy_->GetSelfSource() ? policy_->GetSelfSource()->Clone() : nullptr;
  policy->use_reporting_api = use_reporting_api_;
  policy->report_endpoints = report_endpoints_;
  policy->header = header_.Clone();
  policy->directives = mojo::Clone(directives_);
  policy->upgrade_insecure_requests = upgrade_insecure_requests_;

  return policy;
}

bool CSPDirectiveList::IsObjectRestrictionReasonable() const {
  const network::mojom::blink::CSPSourceList* object_src =
      OperativeDirective(CSPDirectiveName::ObjectSrc).source_list;
  return object_src && CSPSourceListIsNone(*object_src);
}

bool CSPDirectiveList::IsBaseRestrictionReasonable() const {
  const auto base_uri = directives_.find(CSPDirectiveName::BaseURI);
  return (base_uri != directives_.end()) &&
         (CSPSourceListIsNone(*base_uri->value) ||
          CSPSourceListIsSelf(*base_uri->value));
}

bool CSPDirectiveList::IsScriptRestrictionReasonable() const {
  CSPOperativeDirective script_src =
      OperativeDirective(CSPDirectiveName::ScriptSrc);

  // If no `script-src` enforcement occurs, or it allows any and all inline
  // script, the restriction is not reasonable.
  if (!script_src.source_list ||
      CSPSourceListAllowAllInline(script_src.type, *script_src.source_list))
    return false;

  if (CSPSourceListIsNone(*script_src.source_list))
    return true;

  // Policies containing `'strict-dynamic'` are reasonable, as that keyword
  // ensures that host-based expressions and `'unsafe-inline'` are ignored.
  return CSPSourceListIsHashOrNoncePresent(*script_src.source_list) &&
         (script_src.source_list->allow_dynamic ||
          !CSPSourceListAllowsURLBasedMatching(*script_src.source_list));
}

void CSPDirectiveList::Trace(Visitor* visitor) const {
  visitor->Trace(policy_);
}

}  // namespace blink
