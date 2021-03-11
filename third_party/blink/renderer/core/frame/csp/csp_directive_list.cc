// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"

#include <memory>
#include <utility>

#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/require_trusted_types_for_directive.h"
#include "third_party/blink/renderer/core/frame/csp/source_list_directive.h"
#include "third_party/blink/renderer/core/frame/csp/trusted_types_directive.h"
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
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
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

bool ParseBlockAllMixedContent(const String& name,
                               const String& value,
                               ContentSecurityPolicy* policy) {
  if (!value.IsEmpty())
    policy->ReportValueForEmptyDirective(name, value);
  return true;
}

// For "report-uri" directive, this method corresponds to:
// https://w3c.github.io/webappsec-csp/#report-violation
// Step 3.4.2. For each token returned by splitting a string on ASCII whitespace
// with directive's value as the input. [spec text]
//
// For "report-to" directive, the spec says |value| is a single token
// but we use the same logic as "report-uri" and thus we split |value| by
// ASCII whitespaces. The tokens after the first one are discarded in
// ParseReportTo.
// https://w3c.github.io/webappsec-csp/#directive-report-to
Vector<String> ParseReportEndpoints(const String& value) {
  Vector<UChar> characters;
  value.AppendTo(characters);

  // https://infra.spec.whatwg.org/#split-on-ascii-whitespace

  // Step 2. Let tokens be a list of strings, initially empty. [spec text]
  Vector<String> report_endpoints;

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
      report_endpoints.push_back(endpoint);
    }
  }

  return report_endpoints;
}

void ParseReportTo(const String& name,
                   const String& value,
                   network::mojom::blink::ContentSecurityPolicy& csp,
                   ContentSecurityPolicy* policy) {
  if (!base::FeatureList::IsEnabled(network::features::kReporting))
    return;

  csp.use_reporting_api = true;
  csp.report_endpoints = ParseReportEndpoints(value);

  if (csp.report_endpoints.size() > 1) {
    // The directive "report-to" only accepts one endpoint.
    csp.report_endpoints.Shrink(1);
    policy->ReportMultipleReportToEndpoints();
  }
}

void ParseReportURI(const String& name,
                    const String& value,
                    const SecurityOrigin& self_origin,
                    network::mojom::blink::ContentSecurityPolicy& csp,
                    ContentSecurityPolicy* policy) {
  // report-to supersedes report-uri
  if (csp.use_reporting_api)
    return;

  // Remove report-uri in meta policies, per
  // https://html.spec.whatwg.org/C/#attr-meta-http-equiv-content-security-policy.
  if (csp.header->source == ContentSecurityPolicySource::kMeta) {
    policy->ReportInvalidDirectiveInMeta(name);
    return;
  }

  csp.report_endpoints = ParseReportEndpoints(value);

  policy->Count(csp.report_endpoints.size() > 1
                    ? WebFeature::kReportUriMultipleEndpoints
                    : WebFeature::kReportUriSingleEndpoint);

  csp.report_endpoints.erase(
      std::remove_if(csp.report_endpoints.begin(), csp.report_endpoints.end(),
                     [policy, &self_origin](const String& endpoint) {
                       KURL parsed_endpoint = KURL(endpoint);
                       if (!parsed_endpoint.IsValid()) {
                         // endpoint is not absolute, so it cannot violate
                         // MixedContent
                         return false;
                       }
                       if (MixedContentChecker::IsMixedContent(
                               self_origin.Protocol(), parsed_endpoint)) {
                         policy->ReportMixedContentReportURI(endpoint);
                         return true;
                       }
                       return false;
                     }),
      csp.report_endpoints.end());
}

void ParseTreatAsPublicAddress(
    network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy) {
  // Remove treat-as-public-address directives in meta policies, per
  // https://wicg.github.io/cors-rfc1918/#csp
  if (csp.header->source == ContentSecurityPolicySource::kMeta) {
    policy->ReportInvalidDirectiveInMeta("treat-as-public-address");
    return;
  }

  // Remove treat-as-public-address directives in report-only, per
  // https://wicg.github.io/cors-rfc1918/#csp
  if (CSPDirectiveListIsReportOnly(csp)) {
    policy->ReportInvalidInReportOnly("treat-as-public-address");
    return;
  }
  csp.treat_as_public_address = true;
}

void ParseSandboxPolicy(const String& name,
                        const String& sandbox_policy,
                        network::mojom::blink::ContentSecurityPolicy& csp,
                        ContentSecurityPolicy* policy) {
  // Remove sandbox directives in meta policies, per
  // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
  if (csp.header->source == ContentSecurityPolicySource::kMeta) {
    policy->ReportInvalidDirectiveInMeta(name);
    return;
  }
  if (CSPDirectiveListIsReportOnly(csp)) {
    policy->ReportInvalidInReportOnly(name);
    return;
  }

  using network::mojom::blink::WebSandboxFlags;
  WebSandboxFlags ignored_flags =
      !RuntimeEnabledFeatures::StorageAccessAPIEnabled()
          ? WebSandboxFlags::kStorageAccessByUserActivation
          : WebSandboxFlags::kNone;

  network::WebSandboxFlagsParsingResult parsed =
      network::ParseWebSandboxPolicy(sandbox_policy.Utf8(), ignored_flags);
  csp.sandbox = parsed.flags;
  if (!parsed.error_message.empty()) {
    policy->ReportInvalidSandboxFlags(
        WebString::FromUTF8(parsed.error_message));
  }
}

void ParseUpgradeInsecureRequests(
    network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    const String& name,
    const String& value) {
  if (CSPDirectiveListIsReportOnly(csp)) {
    policy->ReportInvalidInReportOnly(name);
    return;
  }
  csp.upgrade_insecure_requests = true;

  if (!value.IsEmpty())
    policy->ReportValueForEmptyDirective(name, value);
}

void AddDirective(const String& name,
                  const String& value,
                  const SecurityOrigin& self_origin,
                  network::mojom::blink::ContentSecurityPolicy& csp,
                  ContentSecurityPolicy* policy) {
  DCHECK(!name.IsEmpty());

  CSPDirectiveName type = ContentSecurityPolicy::GetDirectiveType(name);

  if (type == CSPDirectiveName::Unknown) {
    policy->ReportUnsupportedDirective(name);
    return;
  }

  if (!csp.raw_directives.insert(type, value).is_new_entry) {
    policy->ReportDuplicateDirective(name);
    return;
  }

  network::mojom::blink::CSPSourceListPtr source_list = nullptr;

  switch (type) {
    case CSPDirectiveName::BaseURI:
    case CSPDirectiveName::ChildSrc:
    case CSPDirectiveName::ConnectSrc:
    case CSPDirectiveName::DefaultSrc:
    case CSPDirectiveName::FontSrc:
    case CSPDirectiveName::FormAction:
    case CSPDirectiveName::FrameSrc:
    case CSPDirectiveName::ImgSrc:
    case CSPDirectiveName::ManifestSrc:
    case CSPDirectiveName::MediaSrc:
    case CSPDirectiveName::NavigateTo:
    case CSPDirectiveName::ObjectSrc:
    case CSPDirectiveName::ScriptSrc:
    case CSPDirectiveName::ScriptSrcAttr:
    case CSPDirectiveName::ScriptSrcElem:
    case CSPDirectiveName::StyleSrc:
    case CSPDirectiveName::StyleSrcAttr:
    case CSPDirectiveName::StyleSrcElem:
    case CSPDirectiveName::WorkerSrc:
      csp.directives.insert(type, CSPSourceListParse(name, value, policy));
      return;
    case CSPDirectiveName::FrameAncestors:
      // Remove frame-ancestors directives in meta policies, per
      // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
      if (csp.header->source == ContentSecurityPolicySource::kMeta) {
        policy->ReportInvalidDirectiveInMeta(name);
      } else {
        csp.directives.insert(type, CSPSourceListParse(name, value, policy));
      }
      return;
    case CSPDirectiveName::PrefetchSrc:
      if (!policy->ExperimentalFeaturesEnabled())
        policy->ReportUnsupportedDirective(name);
      else
        csp.directives.insert(type, CSPSourceListParse(name, value, policy));
      return;
    case CSPDirectiveName::BlockAllMixedContent:
      csp.block_all_mixed_content =
          ParseBlockAllMixedContent(name, value, policy);
      return;
    case CSPDirectiveName::ReportTo:
      ParseReportTo(name, value, csp, policy);
      return;
    case CSPDirectiveName::ReportURI:
      ParseReportURI(name, value, self_origin, csp, policy);
      return;
    case CSPDirectiveName::RequireTrustedTypesFor:
      csp.require_trusted_types_for =
          CSPRequireTrustedTypesForParse(value, policy);
      return;
    case CSPDirectiveName::Sandbox:
      ParseSandboxPolicy(name, value, csp, policy);
      return;
    case CSPDirectiveName::TreatAsPublicAddress:
      ParseTreatAsPublicAddress(csp, policy);
      return;
    case CSPDirectiveName::TrustedTypes:
      csp.trusted_types = CSPTrustedTypesParse(value, policy);
      return;
    case CSPDirectiveName::UpgradeInsecureRequests:
      ParseUpgradeInsecureRequests(csp, policy, name, value);
      return;
    case CSPDirectiveName::Unknown:
      NOTREACHED();
      return;
  }
}

// directive         = *WSP [ directive-name [ WSP directive-value ] ]
// directive-name    = 1*( ALPHA / DIGIT / "-" )
// directive-value   = *( WSP / <VCHAR except ";"> )
//
bool ParseDirective(const UChar* begin,
                    const UChar* end,
                    String* name,
                    String* value,
                    ContentSecurityPolicy* policy) {
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
    policy->Count(WebFeature::kMalformedCSP);

    SkipWhile<UChar, IsNotASCIISpace>(position, end);
    policy->ReportUnsupportedDirective(
        String(name_begin, static_cast<wtf_size_t>(position - name_begin)));
    return false;
  }

  *name = String(name_begin, static_cast<wtf_size_t>(position - name_begin))
              .LowerASCII();

  if (position == end)
    return true;

  if (!SkipExactly<UChar, IsASCIISpace>(position, end)) {
    // Malformed CSP: after the directive name we don't have a space
    policy->Count(WebFeature::kMalformedCSP);

    SkipWhile<UChar, IsNotASCIISpace>(position, end);
    policy->ReportUnsupportedDirective(
        String(name_begin, static_cast<wtf_size_t>(position - name_begin)));
    return false;
  }

  SkipWhile<UChar, IsASCIISpace>(position, end);

  const UChar* value_begin = position;
  SkipWhile<UChar, IsCSPDirectiveValueCharacter>(position, end);

  if (position != end) {
    // Malformed CSP: directive value has invalid characters
    policy->Count(WebFeature::kMalformedCSP);

    policy->ReportInvalidDirectiveValueCharacter(
        *name, String(value_begin, static_cast<wtf_size_t>(end - value_begin)));
    return false;
  }

  // The directive-value may be empty.
  if (value_begin == position)
    return true;

  *value = String(value_begin, static_cast<wtf_size_t>(position - value_begin));
  return true;
}

// policy            = directive-list
// directive-list    = [ directive *( ";" [ directive ] ) ]
//
void Parse(const UChar* begin,
           const UChar* end,
           const SecurityOrigin& self_origin,
           bool should_parse_wasm_eval,
           network::mojom::blink::ContentSecurityPolicy& csp,
           ContentSecurityPolicy* policy) {
  if (begin == end)
    return;

  const UChar* position = begin;
  while (position < end) {
    const UChar* directive_begin = position;
    SkipUntil<UChar>(position, end, ';');

    // |name| and |value| must always be initialized in order to avoid mojo
    // serialization errors.
    String name, value = "";
    if (ParseDirective(directive_begin, position, &name, &value, policy)) {
      DCHECK(!name.IsEmpty());
      AddDirective(name, value, self_origin, csp, policy);
    }

    DCHECK(position == end || *position == ';');
    SkipExactly<UChar>(position, end, ';');
  }
}

void ReportViolation(const network::mojom::blink::ContentSecurityPolicy& csp,
                     ContentSecurityPolicy* policy,
                     const String& directive_text,
                     CSPDirectiveName effective_type,
                     const String& console_message,
                     const KURL& blocked_url,
                     ResourceRequest::RedirectStatus redirect_status,
                     ContentSecurityPolicy::ContentSecurityPolicyViolationType
                         violation_type = ContentSecurityPolicy::kURLViolation,
                     const String& sample = String(),
                     const String& sample_prefix = String()) {
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
                          redirect_status,
                          nullptr,  // Element*
                          sample, sample_prefix);
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
      SourceLocation::Capture(context_url, context_line.OneBasedInt(), 0);
  policy->LogToConsole(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError, message, source_location->Clone()));
  policy->ReportViolation(directive_text, effective_type, message, blocked_url,
                          csp.report_endpoints, csp.use_reporting_api,
                          csp.header->header_value, csp.header->type,
                          ContentSecurityPolicy::kInlineViolation,
                          std::move(source_location), nullptr,  // localFrame
                          RedirectStatus::kNoRedirect, element, source);
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
  policy->ReportViolation(directive_text, effective_type, message, blocked_url,
                          csp.report_endpoints, csp.use_reporting_api,
                          csp.header->header_value, csp.header->type,
                          ContentSecurityPolicy::kEvalViolation,
                          std::unique_ptr<SourceLocation>(), nullptr,
                          RedirectStatus::kNoRedirect, nullptr, content);
}

bool CheckEval(const network::mojom::blink::CSPSourceList* directive) {
  return !directive || directive->allow_eval;
}

bool CheckWasmEval(const network::mojom::blink::CSPSourceList* directive,
                   const ContentSecurityPolicy* policy) {
  return !directive || directive->allow_eval ||
         (policy->SupportsWasmEval() && directive->allow_wasm_eval);
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

bool CheckSource(ContentSecurityPolicy* policy,
                 const network::mojom::blink::CSPSourceList* directive,
                 const network::mojom::blink::CSPSource& self_origin,
                 const KURL& url,
                 ResourceRequest::RedirectStatus redirect_status) {
  // If |url| is empty, fall back to the policy URL to ensure that <object>'s
  // without a `src` can be blocked/allowed, as they can still load plugins
  // even though they don't actually have a URL.
  if (!directive)
    return true;

  return CSPSourceListAllows(
      *directive, self_origin,
      url.IsEmpty() ? policy->FallbackUrlForPlugin() : url, redirect_status);
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
  CSPOperativeDirective directive =
      OperativeDirective(csp, CSPDirectiveName::ScriptSrc);
  if (CheckWasmEval(directive.source_list, policy))
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
      CSPSourceListAllowAllInline(directive.type, *directive.source_list))
    return true;

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
               "'unsafe-hashes' keyword' is present.";
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

bool CheckSourceAndReportViolation(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    CSPOperativeDirective directive,
    const KURL& url,
    CSPDirectiveName effective_type,
    const KURL& url_before_redirects,
    ResourceRequest::RedirectStatus redirect_status) {
  if (!directive.source_list)
    return true;

  // We ignore URL-based allowlists if we're allowing dynamic script injection.
  if (CheckSource(policy, directive.source_list, *csp.self_origin, url,
                  redirect_status) &&
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
      GetRawDirectiveForMessage(csp.raw_directives, directive.type);
  ReportViolation(csp, policy, raw_directive, effective_type,
                  prefix + url.ElidedString() +
                      "' because it violates the following Content Security "
                      "Policy directive: \"" +
                      raw_directive + "\"." + suffix + "\n",
                  url_before_redirects, redirect_status);
  return CSPDirectiveListIsReportOnly(csp);
}

bool AllowDynamicWorker(
    const network::mojom::blink::ContentSecurityPolicy& csp) {
  const network::mojom::blink::CSPSourceList* worker_src =
      OperativeDirective(csp, CSPDirectiveName::WorkerSrc).source_list;
  return CheckDynamic(worker_src, CSPDirectiveName::WorkerSrc);
}

network::mojom::blink::CSPSourcePtr ComputeSelfOrigin(
    const SecurityOrigin& self_origin) {
  DCHECK(self_origin.Protocol());
  return network::mojom::blink::CSPSource::New(
      self_origin.Protocol(),
      // Forget the host for file schemes. Host can anyway only be `localhost`
      // or empty and this is platform dependent.
      //
      // TODO(antoniosartori): Consider returning mojom::CSPSource::New() for
      // file: urls, so that 'self' for file: would match nothing.
      self_origin.Protocol() == "file" ? "" : self_origin.Host(),
      self_origin.Port() == DefaultPortForProtocol(self_origin.Protocol())
          ? url::PORT_UNSPECIFIED
          : self_origin.Port(),
      "",
      /*is_host_wildcard=*/false, /*is_port_wildcard=*/false);
}

}  // namespace

network::mojom::blink::ContentSecurityPolicyPtr CSPDirectiveListParse(
    ContentSecurityPolicy* policy,
    const UChar* begin,
    const UChar* end,
    const SecurityOrigin& self_origin,
    ContentSecurityPolicyType type,
    ContentSecurityPolicySource source,
    bool should_parse_wasm_eval) {
  auto csp = network::mojom::blink::ContentSecurityPolicy::New();
  csp->header = network::mojom::blink::ContentSecurityPolicyHeader::New(
      String(begin, static_cast<wtf_size_t>(end - begin)).StripWhiteSpace(),
      type, source);

  const SecurityOrigin& real_self_origin =
      *(self_origin.GetOriginOrPrecursorOriginIfOpaque());
  csp->self_origin = ComputeSelfOrigin(real_self_origin);

  Parse(begin, end, real_self_origin, should_parse_wasm_eval, *csp, policy);
  return csp;
}

bool CSPDirectiveListIsReportOnly(
    const network::mojom::blink::ContentSecurityPolicy& csp) {
  return csp.header->type == network::mojom::ContentSecurityPolicyType::kReport;
}

bool CSPDirectiveListAllowTrustedTypeAssignmentFailure(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    const String& message,
    const String& sample,
    const String& sample_prefix) {
  if (!CSPDirectiveListRequiresTrustedTypes(csp))
    return true;

  ReportViolation(csp, policy,
                  ContentSecurityPolicy::GetDirectiveName(
                      CSPDirectiveName::RequireTrustedTypesFor),
                  CSPDirectiveName::RequireTrustedTypesFor, message, KURL(),
                  RedirectStatus::kNoRedirect,
                  ContentSecurityPolicy::kTrustedTypesSinkViolation, sample,
                  sample_prefix);
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
  CSPDirectiveName type =
      GetDirectiveTypeForAllowInlineFromInlineType(inline_type);

  CSPOperativeDirective directive = OperativeDirective(csp, type);
  if (IsMatchingNoncePresent(directive.source_list, nonce))
    return true;

  auto* html_script_element = DynamicTo<HTMLScriptElement>(element);
  if (html_script_element &&
      inline_type == ContentSecurityPolicy::InlineType::kScript &&
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
        csp, policy, directive,
        "Refused to " + message +
            " because it violates the following Content Security Policy "
            "directive: ",
        element, content, context_url, context_line, inline_type, hash_value,
        type);
  }

  return !directive.source_list ||
         CSPSourceListAllowAllInline(directive.type, *directive.source_list);
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

bool CSPDirectiveListAllowWasmCodeGeneration(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    ReportingDisposition reporting_disposition,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) {
  if (reporting_disposition == ReportingDisposition::kReport) {
    String infix = policy->SupportsWasmEval()
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
  return CSPDirectiveListIsReportOnly(csp) ||
         CheckWasmEval(
             OperativeDirective(csp, CSPDirectiveName::ScriptSrc).source_list,
             policy);
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

bool CSPDirectiveListAllowFromSource(
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

  if (type == CSPDirectiveName::WorkerSrc && AllowDynamicWorker(csp))
    return true;

  if (type == CSPDirectiveName::ScriptSrcElem ||
      type == CSPDirectiveName::StyleSrcElem) {
    if (IsMatchingNoncePresent(OperativeDirective(csp, type).source_list,
                               nonce))
      return true;
  }

  if (type == CSPDirectiveName::ScriptSrcElem) {
    if (parser_disposition == kNotParserInserted &&
        CSPDirectiveListAllowDynamic(csp, type))
      return true;
    if (AreAllMatchingHashesPresent(OperativeDirective(csp, type).source_list,
                                    hashes))
      return true;
  }

  CSPOperativeDirective directive = OperativeDirective(csp, type);
  bool result =
      reporting_disposition == ReportingDisposition::kReport
          ? CheckSourceAndReportViolation(csp, policy, directive, url, type,
                                          url_before_redirects, redirect_status)
          : CheckSource(policy, directive.source_list, *csp.self_origin, url,
                        redirect_status);

  if (type == CSPDirectiveName::BaseURI) {
    if (result && !CheckSource(policy, directive.source_list, *csp.self_origin,
                               url, redirect_status)) {
      policy->Count(WebFeature::kBaseWouldBeBlockedByDefaultSrc);
    }
  }

  return result;
}

bool CSPDirectiveListAllowTrustedTypePolicy(
    const network::mojom::blink::ContentSecurityPolicy& csp,
    ContentSecurityPolicy* policy,
    const String& policy_name,
    bool is_duplicate,
    ContentSecurityPolicy::AllowTrustedTypePolicyDetails& violation_details) {
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
  ReportViolation(csp, policy, "trusted-types", CSPDirectiveName::TrustedTypes,
                  String::Format(message, policy_name.Utf8().c_str(),
                                 raw_directive.Utf8().c_str()),
                  KURL(), RedirectStatus::kNoRedirect,
                  ContentSecurityPolicy::kTrustedTypesPolicyViolation,
                  policy_name);

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
      GetDirectiveTypeForAllowHashFromInlineType(inline_type);
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
