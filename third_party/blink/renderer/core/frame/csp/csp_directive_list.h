// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_DIRECTIVE_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_DIRECTIVE_LIST_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/media_list_directive.h"
#include "third_party/blink/renderer/core/frame/csp/require_trusted_types_for_directive.h"
#include "third_party/blink/renderer/core/frame/csp/source_list_directive.h"
#include "third_party/blink/renderer/core/frame/csp/trusted_types_directive.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ContentSecurityPolicy;
enum class ResourceType : uint8_t;

struct CSPOperativeDirective {
  CSPDirectiveName type;
  const network::mojom::blink::CSPSourceList* source_list;
};

class CORE_EXPORT CSPDirectiveList final
    : public GarbageCollected<CSPDirectiveList> {
 public:
  static CSPDirectiveList* Create(ContentSecurityPolicy*,
                                  const UChar* begin,
                                  const UChar* end,
                                  network::mojom::ContentSecurityPolicyType,
                                  network::mojom::ContentSecurityPolicySource,
                                  bool should_parse_wasm_eval = false);

  CSPDirectiveList(ContentSecurityPolicy*,
                   network::mojom::ContentSecurityPolicyType,
                   network::mojom::ContentSecurityPolicySource);

  void Parse(const UChar* begin,
             const UChar* end,
             bool should_parse_wasm_eval = false);

  const String& Header() const { return header_; }
  network::mojom::ContentSecurityPolicyType HeaderType() const {
    return header_type_;
  }
  network::mojom::ContentSecurityPolicySource HeaderSource() const {
    return header_source_;
  }

  bool AllowInline(ContentSecurityPolicy::InlineType,
                   Element*,
                   const String& content,
                   const String& nonce,
                   const String& context_url,
                   const WTF::OrdinalNumber& context_line,
                   ReportingDisposition) const;

  // Returns whether or not the Javascript code generation should call back the
  // CSP checker before any script evaluation from a string is being made.
  bool ShouldCheckEval() const;

  bool AllowEval(ReportingDisposition,
                 ContentSecurityPolicy::ExceptionStatus,
                 const String& script_content) const;
  bool AllowWasmEval(ReportingDisposition,
                     ContentSecurityPolicy::ExceptionStatus,
                     const String& script_content) const;
  bool AllowPluginType(const String& type,
                       const String& type_attribute,
                       const KURL&,
                       ReportingDisposition) const;

  bool AllowFromSource(CSPDirectiveName,
                       const KURL&,
                       const KURL& url_before_redirects,
                       ResourceRequest::RedirectStatus,
                       ReportingDisposition,
                       const String& nonce = String(),
                       const IntegrityMetadataSet& = IntegrityMetadataSet(),
                       ParserDisposition = kParserInserted) const;

  bool AllowTrustedTypePolicy(
      const String& policy_name,
      bool is_duplicate,
      ContentSecurityPolicy::AllowTrustedTypePolicyDetails& violation_details)
      const;

  bool AllowDynamic(CSPDirectiveName) const;
  bool AllowDynamicWorker() const;

  bool AllowTrustedTypeAssignmentFailure(const String& message,
                                         const String& sample,
                                         const String& sample_prefix) const;

  bool StrictMixedContentChecking() const {
    return strict_mixed_content_checking_enforced_;
  }
  void ReportMixedContent(const KURL& blocked_url,
                          ResourceRequest::RedirectStatus) const;

  bool ShouldDisableEval() const {
    return ShouldDisableEvalBecauseScriptSrc() ||
           ShouldDisableEvalBecauseTrustedTypes();
  }
  bool ShouldDisableEvalBecauseScriptSrc() const;
  bool ShouldDisableEvalBecauseTrustedTypes() const;
  const String& EvalDisabledErrorMessage() const {
    return eval_disabled_error_message_;
  }
  bool IsReportOnly() const {
    return header_type_ == network::mojom::ContentSecurityPolicyType::kReport;
  }
  bool IsActiveForConnections() const {
    return OperativeDirective(CSPDirectiveName::ConnectSrc).source_list;
  }
  const Vector<String>& ReportEndpoints() const { return report_endpoints_; }
  bool UseReportingApi() const { return use_reporting_api_; }

  // Used to copy plugin-types into a plugin document in a nested
  // browsing context.
  bool HasPluginTypes() const { return plugin_types_.has_value(); }
  String PluginTypesText() const;

  bool ShouldSendCSPHeader(ResourceType) const;

  bool AllowHash(const network::mojom::blink::CSPHashSource& hash_value,
                 const ContentSecurityPolicy::InlineType inline_type) const;

  // Export a subset of the Policy. The primary goal of this method is to make
  // the embedders aware of the directives that affect navigation, as the
  // embedder is responsible for navigational enforcement.
  // It currently contains the following ones:
  // * default-src
  // * child-src
  // * frame-src
  // * form-action
  // * upgrade-insecure-requests
  // * navigate-to
  // The exported directives only contains sources that affect navigation. For
  // instance it doesn't contains 'unsafe-inline' or 'unsafe-eval'
  network::mojom::blink::ContentSecurityPolicyPtr ExposeForNavigationalChecks()
      const;

  // We consider `object-src` restrictions to be reasonable iff they're
  // equivalent to `object-src 'none'`.
  bool IsObjectRestrictionReasonable() const;

  // We consider `base-uri` restrictions to be reasonable iff they're equivalent
  // to `base-uri 'none'` or `base-uri 'self'`.
  bool IsBaseRestrictionReasonable() const;

  // We consider `script-src` restrictions to be reasonable iff they're not
  // URL-based (e.g. they contain only nonces and hashes, or they use
  // 'strict-dynamic'). Neither `'unsafe-eval'` nor `'unsafe-hashes'` affect
  // this judgement.
  bool IsScriptRestrictionReasonable() const;

  bool RequiresTrustedTypes() const;

  bool TrustedTypesAllowDuplicates() const {
    return trusted_types_ && trusted_types_->allow_duplicates;
  }

  void Trace(Visitor*) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(CSPDirectiveListTest, IsMatchingNoncePresent);
  FRIEND_TEST_ALL_PREFIXES(CSPDirectiveListTest, OperativeDirectiveGivenType);

  bool ParseDirective(const UChar* begin,
                      const UChar* end,
                      String* name,
                      String* value);
  void ParseReportURI(const String& name, const String& value);
  void ParseReportTo(const String& name, const String& value);
  void ParseAndAppendReportEndpoints(const String& value);
  void ParsePluginTypes(const String& name, const String& value);
  void AddDirective(const String& name, const String& value);
  void ApplySandboxPolicy(const String& name, const String& sandbox_policy);
  void ApplyTreatAsPublicAddress();
  void EnforceStrictMixedContentChecking(const String& name,
                                         const String& value);
  void EnableInsecureRequestsUpgrade(const String& name, const String& value);

  CSPDirectiveName FallbackDirective(CSPDirectiveName current_directive,
                                     CSPDirectiveName original_directive) const;
  void ReportViolation(
      const String& directive_text,
      CSPDirectiveName,
      const String& console_message,
      const KURL& blocked_url,
      ResourceRequest::RedirectStatus,
      ContentSecurityPolicy::ContentSecurityPolicyViolationType violation_type =
          ContentSecurityPolicy::kURLViolation,
      const String& sample = String(),
      const String& sample_prefix = String()) const;
  void ReportViolationWithLocation(const String& directive_text,
                                   CSPDirectiveName,
                                   const String& console_message,
                                   const KURL& blocked_url,
                                   const String& context_url,
                                   const WTF::OrdinalNumber& context_line,
                                   Element*,
                                   const String& source) const;
  void ReportEvalViolation(const String& directive_text,
                           CSPDirectiveName,
                           const String& message,
                           const KURL& blocked_url,
                           const ContentSecurityPolicy::ExceptionStatus,
                           const String& content) const;

  bool CheckEval(const network::mojom::blink::CSPSourceList* directive) const;
  bool CheckWasmEval(
      const network::mojom::blink::CSPSourceList* directive) const;
  bool CheckDynamic(const network::mojom::blink::CSPSourceList* directive,
                    CSPDirectiveName effective_type) const;
  bool IsMatchingNoncePresent(
      const network::mojom::blink::CSPSourceList* directive,
      const String&) const;
  bool AreAllMatchingHashesPresent(
      const network::mojom::blink::CSPSourceList* directive,
      const IntegrityMetadataSet&) const;
  bool CheckHash(const network::mojom::blink::CSPSourceList* directive,
                 const network::mojom::blink::CSPHashSource&) const;
  bool CheckUnsafeHashesAllowed(
      const network::mojom::blink::CSPSourceList* directive) const;
  bool CheckSource(const network::mojom::blink::CSPSourceList*,
                   const KURL&,
                   ResourceRequest::RedirectStatus) const;
  bool CheckMediaType(const Vector<String>& plugin_types,
                      const String& type,
                      const String& type_attribute) const;

  void SetEvalDisabledErrorMessage(const String& error_message) {
    eval_disabled_error_message_ = error_message;
  }

  bool CheckEvalAndReportViolation(const String& console_message,
                                   ContentSecurityPolicy::ExceptionStatus,
                                   const String& script_content) const;
  bool CheckWasmEvalAndReportViolation(const String& console_message,
                                       ContentSecurityPolicy::ExceptionStatus,
                                       const String& script_content) const;
  bool CheckInlineAndReportViolation(CSPOperativeDirective directive,
                                     const String& console_message,
                                     Element*,
                                     const String& source,
                                     const String& context_url,
                                     const WTF::OrdinalNumber& context_line,
                                     bool is_script,
                                     const String& hash_value,
                                     CSPDirectiveName effective_type) const;

  bool CheckSourceAndReportViolation(CSPOperativeDirective directive,
                                     const KURL&,
                                     CSPDirectiveName,
                                     const KURL& url_before_redirects,
                                     ResourceRequest::RedirectStatus) const;
  bool CheckMediaTypeAndReportViolation(const Vector<String>& plugin_types,
                                        const String& type,
                                        const String& type_attribute,
                                        const String& console_message) const;

  bool DenyIfEnforcingPolicy() const { return IsReportOnly(); }

  // Return the operative directive name and CSPSourceList for a given directive
  // name, falling back to generic directives according to Content Security
  // Policies rules. For example, if 'default-src' is defined but 'media-src' is
  // not, OperativeDirective(CSPDirectiveName::MediaSrc) will return type
  // CSPDirectiveName::DefaultSrc and the corresponding CSPSourceList. If no
  // operative directive for the given type is defined, this will return
  // CSPDirectiveName::Unknown and nullptr.
  CSPOperativeDirective OperativeDirective(
      CSPDirectiveName type,
      CSPDirectiveName original_type = CSPDirectiveName::Unknown) const;

  Member<ContentSecurityPolicy> policy_;

  String header_;
  network::mojom::ContentSecurityPolicyType header_type_;
  network::mojom::ContentSecurityPolicySource header_source_;

  HashMap<CSPDirectiveName, String> raw_directives_;

  bool has_sandbox_policy_;

  bool strict_mixed_content_checking_enforced_;

  bool upgrade_insecure_requests_;

  base::Optional<Vector<String>> plugin_types_;
  network::mojom::blink::CSPSourceListPtr base_uri_;
  network::mojom::blink::CSPSourceListPtr child_src_;
  network::mojom::blink::CSPSourceListPtr connect_src_;
  network::mojom::blink::CSPSourceListPtr default_src_;
  network::mojom::blink::CSPSourceListPtr font_src_;
  network::mojom::blink::CSPSourceListPtr form_action_;
  network::mojom::blink::CSPSourceListPtr frame_ancestors_;
  network::mojom::blink::CSPSourceListPtr frame_src_;
  network::mojom::blink::CSPSourceListPtr img_src_;
  network::mojom::blink::CSPSourceListPtr media_src_;
  network::mojom::blink::CSPSourceListPtr manifest_src_;
  network::mojom::blink::CSPSourceListPtr object_src_;
  network::mojom::blink::CSPSourceListPtr prefetch_src_;
  network::mojom::blink::CSPSourceListPtr script_src_;
  network::mojom::blink::CSPSourceListPtr script_src_attr_;
  network::mojom::blink::CSPSourceListPtr script_src_elem_;
  network::mojom::blink::CSPSourceListPtr style_src_;
  network::mojom::blink::CSPSourceListPtr style_src_attr_;
  network::mojom::blink::CSPSourceListPtr style_src_elem_;
  network::mojom::blink::CSPSourceListPtr worker_src_;
  network::mojom::blink::CSPSourceListPtr navigate_to_;
  network::mojom::blink::CSPTrustedTypesPtr trusted_types_;
  network::mojom::blink::CSPRequireTrustedTypesFor require_trusted_types_for_;

  // If a "report-to" directive is used:
  // - |report_endpoints_| is a list of token parsed from the "report-to"
  //   directive's value, and
  // - |use_reporting_api_| is true.
  // Otherwise,
  // - |report_endpoints_| is a list of uri-reference parsed from a
  //   "report-uri" directive's value if any, and
  // - |use_reporting_api_| is false.
  Vector<String> report_endpoints_;
  bool use_reporting_api_;

  String eval_disabled_error_message_;

  DISALLOW_COPY_AND_ASSIGN(CSPDirectiveList);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_DIRECTIVE_LIST_H_
