// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_DIRECTIVE_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_DIRECTIVE_LIST_H_

#include "base/macros.h"
#include "third_party/blink/public/platform/web_content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/media_list_directive.h"
#include "third_party/blink/renderer/core/frame/csp/source_list_directive.h"
#include "third_party/blink/renderer/core/frame/csp/string_list_directive.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_violation_reporting_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ContentSecurityPolicy;
enum class ResourceType : uint8_t;

typedef HeapVector<Member<SourceListDirective>> SourceListDirectiveVector;

class CORE_EXPORT CSPDirectiveList final
    : public GarbageCollected<CSPDirectiveList> {
 public:
  static CSPDirectiveList* Create(ContentSecurityPolicy*,
                                  const UChar* begin,
                                  const UChar* end,
                                  ContentSecurityPolicyHeaderType,
                                  ContentSecurityPolicyHeaderSource,
                                  bool should_parse_wasm_eval = false);

  CSPDirectiveList(ContentSecurityPolicy*,
                   ContentSecurityPolicyHeaderType,
                   ContentSecurityPolicyHeaderSource);

  void Parse(const UChar* begin,
             const UChar* end,
             bool should_parse_wasm_eval = false);

  const String& Header() const { return header_; }
  ContentSecurityPolicyHeaderType HeaderType() const { return header_type_; }
  ContentSecurityPolicyHeaderSource HeaderSource() const {
    return header_source_;
  }

  bool AllowInline(ContentSecurityPolicy::InlineType,
                   Element*,
                   const String& content,
                   const String& nonce,
                   const String& context_url,
                   const WTF::OrdinalNumber& context_line,
                   SecurityViolationReportingPolicy) const;

  bool AllowEval(SecurityViolationReportingPolicy,
                 ContentSecurityPolicy::ExceptionStatus,
                 const String& script_content) const;
  bool AllowWasmEval(SecurityViolationReportingPolicy,
                     ContentSecurityPolicy::ExceptionStatus,
                     const String& script_content) const;
  bool AllowPluginType(const String& type,
                       const String& type_attribute,
                       const KURL&,
                       SecurityViolationReportingPolicy) const;

  bool AllowFromSource(ContentSecurityPolicy::DirectiveType,
                       const KURL&,
                       ResourceRequest::RedirectStatus,
                       SecurityViolationReportingPolicy,
                       const String& nonce = String(),
                       const IntegrityMetadataSet& = IntegrityMetadataSet(),
                       ParserDisposition = kParserInserted) const;

  bool AllowTrustedTypePolicy(const String& policy_name,
                              bool is_duplicate) const;

  // |allowAncestors| does not need to know whether the resource was a
  // result of a redirect. After a redirect, source paths are usually
  // ignored to stop a page from learning the path to which the
  // request was redirected, but this is not a concern for ancestors,
  // because a child frame can't manipulate the URL of a cross-origin
  // parent.
  bool AllowAncestors(LocalFrame*,
                      const KURL&,
                      SecurityViolationReportingPolicy) const;
  bool AllowDynamic(ContentSecurityPolicy::DirectiveType) const;
  bool AllowDynamicWorker() const;

  bool AllowRequestWithoutIntegrity(mojom::RequestContextType,
                                    const KURL&,
                                    ResourceRequest::RedirectStatus,
                                    SecurityViolationReportingPolicy) const;

  bool AllowTrustedTypeAssignmentFailure(const String& message,
                                         const String& sample) const;

  bool StrictMixedContentChecking() const {
    return strict_mixed_content_checking_enforced_;
  }
  void ReportMixedContent(const KURL& mixed_url,
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
    return header_type_ == kContentSecurityPolicyHeaderTypeReport;
  }
  bool IsActiveForConnections() const {
    return OperativeDirective(
        ContentSecurityPolicy::DirectiveType::kConnectSrc);
  }
  const Vector<String>& ReportEndpoints() const { return report_endpoints_; }
  bool UseReportingApi() const { return use_reporting_api_; }
  uint8_t RequireSRIForTokens() const { return require_sri_for_; }
  bool IsFrameAncestorsEnforced() const {
    return frame_ancestors_.Get() && !IsReportOnly();
  }

  // Used to copy plugin-types into a plugin document in a nested
  // browsing context.
  bool HasPluginTypes() const { return !!plugin_types_; }
  const String& PluginTypesText() const;

  bool ShouldSendCSPHeader(ResourceType) const;

  bool AllowHash(const CSPHashValue& hash_value,
                 const ContentSecurityPolicy::InlineType inline_type) const;

  // The algorithm is described here:
  // https://w3c.github.io/webappsec-csp/embedded/#subsume-policy
  bool Subsumes(const CSPDirectiveListVector&);

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
  WebContentSecurityPolicy ExposeForNavigationalChecks() const;

  void Trace(blink::Visitor*);

 private:
  FRIEND_TEST_ALL_PREFIXES(CSPDirectiveListTest, IsMatchingNoncePresent);
  FRIEND_TEST_ALL_PREFIXES(CSPDirectiveListTest, GetSourceVector);
  FRIEND_TEST_ALL_PREFIXES(CSPDirectiveListTest, OperativeDirectiveGivenType);

  enum RequireSRIForToken { kNone = 0, kScript = 1 << 0, kStyle = 1 << 1 };

  bool ParseDirective(const UChar* begin,
                      const UChar* end,
                      String* name,
                      String* value);
  void ParseRequireSRIFor(const String& name, const String& value);
  void ParseReportURI(const String& name, const String& value);
  void ParseReportTo(const String& name, const String& value);
  void ParseAndAppendReportEndpoints(const String& value);
  void ParsePluginTypes(const String& name, const String& value);
  void AddDirective(const String& name, const String& value);
  void ApplySandboxPolicy(const String& name, const String& sandbox_policy);
  void EnforceStrictMixedContentChecking(const String& name,
                                         const String& value);
  void EnableInsecureRequestsUpgrade(const String& name, const String& value);
  void RequireTrustedTypes(const String& name, const String& value);

  template <class CSPDirectiveType>
  void SetCSPDirective(const String& name,
                       const String& value,
                       Member<CSPDirectiveType>&,
                       bool should_parse_wasm_eval = false);

  ContentSecurityPolicy::DirectiveType FallbackDirective(
      const ContentSecurityPolicy::DirectiveType current_directive,
      const ContentSecurityPolicy::DirectiveType original_directive) const;
  void ReportViolation(const String& directive_text,
                       const ContentSecurityPolicy::DirectiveType,
                       const String& console_message,
                       const KURL& blocked_url,
                       ResourceRequest::RedirectStatus,
                       ContentSecurityPolicy::ViolationType violation_type =
                           ContentSecurityPolicy::kURLViolation,
                       const String& sample = String()) const;
  void ReportViolationWithFrame(const String& directive_text,
                                const ContentSecurityPolicy::DirectiveType,
                                const String& console_message,
                                const KURL& blocked_url,
                                LocalFrame*) const;
  void ReportViolationWithLocation(const String& directive_text,
                                   const ContentSecurityPolicy::DirectiveType,
                                   const String& console_message,
                                   const KURL& blocked_url,
                                   const String& context_url,
                                   const WTF::OrdinalNumber& context_line,
                                   Element*,
                                   const String& source) const;
  void ReportEvalViolation(const String& directive_text,
                           const ContentSecurityPolicy::DirectiveType,
                           const String& message,
                           const KURL& blocked_url,
                           const ContentSecurityPolicy::ExceptionStatus,
                           const String& content) const;

  bool CheckEval(SourceListDirective*) const;
  bool CheckWasmEval(SourceListDirective*) const;
  bool CheckDynamic(SourceListDirective*) const;
  bool IsMatchingNoncePresent(SourceListDirective*, const String&) const;
  bool AreAllMatchingHashesPresent(SourceListDirective*,
                                   const IntegrityMetadataSet&) const;
  bool CheckHash(SourceListDirective*, const CSPHashValue&) const;
  bool CheckUnsafeHashesAllowed(SourceListDirective*) const;
  bool CheckSource(SourceListDirective*,
                   const KURL&,
                   ResourceRequest::RedirectStatus) const;
  bool CheckMediaType(MediaListDirective*,
                      const String& type,
                      const String& type_attribute) const;
  bool CheckAncestors(SourceListDirective*, LocalFrame*) const;
  bool CheckRequestWithoutIntegrity(mojom::RequestContextType) const;

  void SetEvalDisabledErrorMessage(const String& error_message) {
    eval_disabled_error_message_ = error_message;
  }

  bool CheckEvalAndReportViolation(SourceListDirective*,
                                   const String& console_message,
                                   ContentSecurityPolicy::ExceptionStatus,
                                   const String& script_content) const;
  bool CheckWasmEvalAndReportViolation(SourceListDirective*,
                                       const String& console_message,
                                       ContentSecurityPolicy::ExceptionStatus,
                                       const String& script_content) const;
  bool CheckInlineAndReportViolation(
      SourceListDirective*,
      const String& console_message,
      Element*,
      const String& source,
      const String& context_url,
      const WTF::OrdinalNumber& context_line,
      bool is_script,
      const String& hash_value,
      ContentSecurityPolicy::DirectiveType effective_type) const;

  bool CheckSourceAndReportViolation(SourceListDirective*,
                                     const KURL&,
                                     const ContentSecurityPolicy::DirectiveType,
                                     ResourceRequest::RedirectStatus) const;
  bool CheckMediaTypeAndReportViolation(MediaListDirective*,
                                        const String& type,
                                        const String& type_attribute,
                                        const String& console_message) const;
  bool CheckAncestorsAndReportViolation(SourceListDirective*,
                                        LocalFrame*,
                                        const KURL&) const;
  bool CheckRequestWithoutIntegrityAndReportViolation(
      mojom::RequestContextType,
      const KURL&,
      ResourceRequest::RedirectStatus) const;

  bool DenyIfEnforcingPolicy() const { return IsReportOnly(); }

  // This function returns a SourceListDirective of a given type
  // or if it is not defined, the fallback SourceListDirective for that type.
  SourceListDirective* OperativeDirective(
      const ContentSecurityPolicy::DirectiveType type,
      ContentSecurityPolicy::DirectiveType original_type =
          ContentSecurityPolicy::DirectiveType::kUndefined) const;

  // This function aggregates from a vector of policies all operative
  // SourceListDirectives of a given type into a vector.
  static SourceListDirectiveVector GetSourceVector(
      const ContentSecurityPolicy::DirectiveType,
      const CSPDirectiveListVector& policies);

  Member<ContentSecurityPolicy> policy_;

  String header_;
  ContentSecurityPolicyHeaderType header_type_;
  ContentSecurityPolicyHeaderSource header_source_;

  bool has_sandbox_policy_;

  bool strict_mixed_content_checking_enforced_;

  bool upgrade_insecure_requests_;

  Member<MediaListDirective> plugin_types_;
  Member<SourceListDirective> base_uri_;
  Member<SourceListDirective> child_src_;
  Member<SourceListDirective> connect_src_;
  Member<SourceListDirective> default_src_;
  Member<SourceListDirective> font_src_;
  Member<SourceListDirective> form_action_;
  Member<SourceListDirective> frame_ancestors_;
  Member<SourceListDirective> frame_src_;
  Member<SourceListDirective> img_src_;
  Member<SourceListDirective> media_src_;
  Member<SourceListDirective> manifest_src_;
  Member<SourceListDirective> object_src_;
  Member<SourceListDirective> prefetch_src_;
  Member<SourceListDirective> script_src_;
  Member<SourceListDirective> script_src_attr_;
  Member<SourceListDirective> script_src_elem_;
  Member<SourceListDirective> style_src_;
  Member<SourceListDirective> style_src_attr_;
  Member<SourceListDirective> style_src_elem_;
  Member<SourceListDirective> worker_src_;
  Member<SourceListDirective> navigate_to_;
  Member<StringListDirective> trusted_types_;

  uint8_t require_sri_for_;

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
