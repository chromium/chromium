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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CONTENT_SECURITY_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CONTENT_SECURITY_POLICY_H_

#include <memory>
#include <utility>

#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_security_policy_struct.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_violation_reporting_policy.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace WTF {
class OrdinalNumber;
}

namespace blink {

class ContentSecurityPolicyResponseHeaders;
class ConsoleMessage;
class CSPDirectiveList;
class CSPSource;
class Document;
class Element;
class ExecutionContext;
class LocalFrameClient;
class LocalFrame;
class KURL;
class ResourceRequest;
class SecurityOrigin;
class SecurityPolicyViolationEventInit;
class SourceLocation;
enum class ResourceType : uint8_t;

using SandboxFlags = WebSandboxFlags;
typedef HeapVector<Member<CSPDirectiveList>> CSPDirectiveListVector;
typedef HeapVector<Member<ConsoleMessage>> ConsoleMessageVector;
typedef std::pair<String, ContentSecurityPolicyHeaderType> CSPHeaderAndType;
using RedirectStatus = ResourceRequest::RedirectStatus;

//  A delegate interface to implement violation reporting, support for some
//  directives and other miscellaneous functionality.
class CORE_EXPORT ContentSecurityPolicyDelegate : public GarbageCollectedMixin {
 public:
  // Returns the SecurityOrigin this content security policy is bound to. Used
  // for matching the 'self' keyword. Must return a non-null value.
  // See https://w3c.github.io/webappsec-csp/#policy-self-origin.
  virtual const SecurityOrigin* GetSecurityOrigin() = 0;

  // Returns the URL this content security policy is bound to.
  // Used for https://w3c.github.io/webappsec-csp/#violation-url and so.
  // Note: Url() is used for several purposes that are specced slightly
  // differently.
  // See comments at the callers.
  virtual const KURL& Url() const = 0;

  // Directives support.
  virtual void SetSandboxFlags(SandboxFlags) = 0;
  virtual void SetRequireTrustedTypes() = 0;
  virtual void AddInsecureRequestPolicy(WebInsecureRequestPolicy) = 0;

  // Violation reporting.

  // See https://w3c.github.io/webappsec-csp/#create-violation-for-global.
  // These functions are used to create the violation object.
  virtual std::unique_ptr<SourceLocation> GetSourceLocation() = 0;
  virtual base::Optional<uint16_t> GetStatusCode() = 0;
  // If the Delegate is not bound to a document, a null string should be
  // returned as the referrer.
  virtual String GetDocumentReferrer() = 0;

  virtual void DispatchViolationEvent(const SecurityPolicyViolationEventInit&,
                                      Element*) = 0;
  virtual void PostViolationReport(const SecurityPolicyViolationEventInit&,
                                   const String& stringified_report,
                                   bool is_frame_ancestors_violaton,
                                   const Vector<String>& report_endpoints,
                                   bool use_reporting_api) = 0;

  virtual void Count(WebFeature) = 0;

  virtual void AddConsoleMessage(ConsoleMessage*) = 0;
  virtual void DisableEval(const String& error_message) = 0;
  virtual void ReportBlockedScriptExecutionToInspector(
      const String& directive_text) = 0;
  virtual void DidAddContentSecurityPolicies(
      const blink::WebVector<WebContentSecurityPolicy>&) = 0;
};

class CORE_EXPORT ContentSecurityPolicy final
    : public GarbageCollected<ContentSecurityPolicy> {
 public:
  enum ExceptionStatus { kWillThrowException, kWillNotThrowException };

  // This covers the possible values of a violation's 'resource', as defined in
  // https://w3c.github.io/webappsec-csp/#violation-resource. By the time we
  // generate a report, we're guaranteed that the value isn't 'null', so we
  // don't need that state in this enum.
  //
  // Trusted Types violation's 'resource' values are defined in
  // https://wicg.github.io/trusted-types/dist/spec/#csp-violation-object-hdr.
  enum ViolationType {
    kInlineViolation,
    kEvalViolation,
    kURLViolation,
    kTrustedTypesSinkViolation,
    kTrustedTypesPolicyViolation
  };

  // The |type| argument given to inline checks, e.g.:
  // https://w3c.github.io/webappsec-csp/#should-block-inline
  // Its possible values are listed in:
  // https://w3c.github.io/webappsec-csp/#effective-directive-for-inline-check
  enum class InlineType {
    kNavigation,
    kScript,
    kScriptAttribute,
    kStyle,
    kStyleAttribute
  };

  enum class DirectiveType {
    kBaseURI,
    kBlockAllMixedContent,
    kChildSrc,
    kConnectSrc,
    kDefaultSrc,
    kFontSrc,
    kFormAction,
    kFrameAncestors,
    kFrameSrc,
    kImgSrc,
    kManifestSrc,
    kMediaSrc,
    kNavigateTo,
    kObjectSrc,
    kPluginTypes,
    kPrefetchSrc,
    kReportTo,
    kReportURI,
    kRequireSRIFor,
    kTrustedTypes,
    kSandbox,
    kScriptSrc,
    kScriptSrcAttr,
    kScriptSrcElem,
    kStyleSrc,
    kStyleSrcAttr,
    kStyleSrcElem,
    kUndefined,
    kUpgradeInsecureRequests,
    kWorkerSrc,
  };

  // CheckHeaderType can be passed to Allow*FromSource methods to control which
  // types of CSP headers are checked.
  enum class CheckHeaderType {
    // Check both Content-Security-Policy and
    // Content-Security-Policy-Report-Only headers.
    kCheckAll,
    // Check Content-Security-Policy headers only and ignore
    // Content-Security-Policy-Report-Only headers.
    kCheckEnforce,
    // Check Content-Security-Policy-Report-Only headers only and ignore
    // Content-Security-Policy headers.
    kCheckReportOnly
  };

  static const size_t kMaxSampleLength = 40;

  ContentSecurityPolicy();
  ~ContentSecurityPolicy();
  void Trace(blink::Visitor*);

  bool IsBound();
  void BindToDelegate(ContentSecurityPolicyDelegate&);
  void SetupSelf(const SecurityOrigin&);
  void SetupSelf(const ContentSecurityPolicy&);
  void CopyStateFrom(const ContentSecurityPolicy*);
  void CopyPluginTypesFrom(const ContentSecurityPolicy*);

  void DidReceiveHeaders(const ContentSecurityPolicyResponseHeaders&);
  void DidReceiveHeader(const String&,
                        ContentSecurityPolicyHeaderType,
                        ContentSecurityPolicyHeaderSource);
  void AddPolicyFromHeaderValue(const String&,
                                ContentSecurityPolicyHeaderType,
                                ContentSecurityPolicyHeaderSource);
  void ReportAccumulatedHeaders(LocalFrameClient*) const;

  Vector<CSPHeaderAndType> Headers() const;

  // When the reporting status is |SendReport|, the |ExceptionStatus|
  // should indicate whether the caller will throw a JavaScript
  // exception in the event of a violation. When the caller will throw
  // an exception, ContentSecurityPolicy does not log a violation
  // message to the console because it would be redundant.
  bool AllowEval(SecurityViolationReportingPolicy,
                 ExceptionStatus,
                 const String& script_content) const;
  bool AllowWasmEval(SecurityViolationReportingPolicy,
                     ExceptionStatus,
                     const String& script_content) const;
  bool AllowPluginType(const String& type,
                       const String& type_attribute,
                       const KURL&,
                       SecurityViolationReportingPolicy =
                           SecurityViolationReportingPolicy::kReport) const;
  // Checks whether the plugin type should be allowed in the given
  // document; enforces the CSP rule that PluginDocuments inherit
  // plugin-types directives from the parent document.
  bool AllowPluginTypeForDocument(
      const Document&,
      const String& type,
      const String& type_attribute,
      const KURL&,
      SecurityViolationReportingPolicy =
          SecurityViolationReportingPolicy::kReport) const;

  // AllowFromSource() wrappers.
  bool AllowBaseURI(const KURL&) const;
  bool AllowConnectToSource(const KURL&,
                            RedirectStatus = RedirectStatus::kNoRedirect,
                            SecurityViolationReportingPolicy =
                                SecurityViolationReportingPolicy::kReport,
                            CheckHeaderType = CheckHeaderType::kCheckAll) const;
  bool AllowFormAction(const KURL&) const;
  bool AllowImageFromSource(const KURL&,
                            RedirectStatus = RedirectStatus::kNoRedirect,
                            SecurityViolationReportingPolicy =
                                SecurityViolationReportingPolicy::kReport,
                            CheckHeaderType = CheckHeaderType::kCheckAll) const;
  bool AllowMediaFromSource(const KURL&) const;
  bool AllowObjectFromSource(const KURL&) const;
  bool AllowScriptFromSource(
      const KURL&,
      const String& nonce,
      const IntegrityMetadataSet&,
      ParserDisposition,
      RedirectStatus = RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy =
          SecurityViolationReportingPolicy::kReport,
      CheckHeaderType = CheckHeaderType::kCheckAll) const;
  bool AllowWorkerContextFromSource(const KURL&) const;

  bool AllowTrustedTypePolicy(const String& policy_name,
                              bool is_duplicate) const;

  // Passing 'String()' into the |nonce| arguments in the following methods
  // represents an unnonced resource load.
  //
  // For kJavaScriptURL case, |element| will not be present for navigations to
  // javascript URLs, as those checks happen in the middle of the navigation
  // algorithm, and we generally don't have access to the responsible element.
  //
  // For kInlineEventHandler case, |element| will be present almost all of the
  // time, but because of strangeness around targeting handlers for '<body>',
  // '<svg>', and '<frameset>', it will be 'nullptr' for handlers on those
  // elements.
  bool AllowInline(InlineType,
                   Element*,
                   const String& content,
                   const String& nonce,
                   const String& context_url,
                   const WTF::OrdinalNumber& context_line,
                   SecurityViolationReportingPolicy =
                       SecurityViolationReportingPolicy::kReport) const;

  static bool IsScriptInlineType(InlineType);

  // |allowAncestors| does not need to know whether the resource was a
  // result of a redirect. After a redirect, source paths are usually
  // ignored to stop a page from learning the path to which the
  // request was redirected, but this is not a concern for ancestors,
  // because a child frame can't manipulate the URL of a cross-origin
  // parent.
  bool AllowAncestors(LocalFrame*,
                      const KURL&,
                      SecurityViolationReportingPolicy =
                          SecurityViolationReportingPolicy::kReport) const;
  bool IsFrameAncestorsEnforced() const;

  bool AllowRequestWithoutIntegrity(
      mojom::RequestContextType,
      const KURL&,
      RedirectStatus = RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy =
          SecurityViolationReportingPolicy::kReport,
      CheckHeaderType = CheckHeaderType::kCheckAll) const;

  bool AllowRequest(mojom::RequestContextType,
                    const KURL&,
                    const String& nonce,
                    const IntegrityMetadataSet&,
                    ParserDisposition,
                    RedirectStatus = RedirectStatus::kNoRedirect,
                    SecurityViolationReportingPolicy =
                        SecurityViolationReportingPolicy::kReport,
                    CheckHeaderType = CheckHeaderType::kCheckAll) const;

  // Determine whether to enforce the assignment failure. Also handle reporting.
  // Returns whether enforcing Trusted Types CSP directives are present.
  bool AllowTrustedTypeAssignmentFailure(const String& message,
                                         const String& sample = String()) const;

  void UsesScriptHashAlgorithms(uint8_t content_security_policy_hash_algorithm);
  void UsesStyleHashAlgorithms(uint8_t content_security_policy_hash_algorithm);

  void SetOverrideAllowInlineStyle(bool);
  void SetOverrideURLForSelf(const KURL&);

  bool IsActive() const;
  bool IsActiveForConnections() const;

  // If a frame is passed in, the message will be logged to its active
  // document's console.  Otherwise, the message will be logged to this object's
  // |m_executionContext|.
  void LogToConsole(ConsoleMessage*, LocalFrame* = nullptr);

  void ReportDirectiveAsSourceExpression(const String& directive_name,
                                         const String& source_expression);
  void ReportDuplicateDirective(const String&);
  void ReportInvalidDirectiveValueCharacter(const String& directive_name,
                                            const String& value);
  void ReportInvalidPathCharacter(const String& directive_name,
                                  const String& value,
                                  const char);
  void ReportInvalidPluginTypes(const String&);
  void ReportInvalidRequireSRIForTokens(const String&);
  void ReportInvalidSandboxFlags(const String&);
  void ReportInvalidSourceExpression(const String& directive_name,
                                     const String& source);
  void ReportMissingReportURI(const String&);
  void ReportUnsupportedDirective(const String&);
  void ReportInvalidInReportOnly(const String&);
  void ReportInvalidDirectiveInMeta(const String& directive_name);
  void ReportReportOnlyInMeta(const String&);
  void ReportMetaOutsideHead(const String&);
  void ReportValueForEmptyDirective(const String& directive_name,
                                    const String& value);

  // If a frame is passed in, the report will be sent using it as a context. If
  // no frame is passed in, the report will be sent via this object's
  // |m_executionContext| (or dropped on the floor if no such context is
  // available).
  // If |sourceLocation| is not set, the source location will be the context's
  // current location.
  void ReportViolation(const String& directive_text,
                       const DirectiveType& effective_type,
                       const String& console_message,
                       const KURL& blocked_url,
                       const Vector<String>& report_endpoints,
                       bool use_reporting_api,
                       const String& header,
                       ContentSecurityPolicyHeaderType,
                       ViolationType,
                       std::unique_ptr<SourceLocation>,
                       LocalFrame* = nullptr,
                       RedirectStatus = RedirectStatus::kFollowedRedirect,
                       Element* = nullptr,
                       const String& source = g_empty_string);

  // Called when mixed content is detected on a page; will trigger a violation
  // report if the 'block-all-mixed-content' directive is specified for a
  // policy.
  void ReportMixedContent(const KURL& mixed_url, RedirectStatus) const;

  void ReportBlockedScriptExecutionToInspector(
      const String& directive_text) const;

  // Used as <object>'s URL when there is no `src` attribute.
  const KURL FallbackUrlForPlugin() const;

  void EnforceSandboxFlags(SandboxFlags);
  void RequireTrustedTypes();
  bool IsRequireTrustedTypes() const { return require_trusted_types_; }
  String EvalDisabledErrorMessage() const;

  // Upgrade-Insecure-Requests and Block-All-Mixed-Content are represented in
  // |m_insecureRequestPolicy|
  void EnforceStrictMixedContentChecking();
  void UpgradeInsecureRequests();
  WebInsecureRequestPolicy GetInsecureRequestPolicy() const {
    return insecure_request_policy_;
  }

  bool UrlMatchesSelf(const KURL&) const;
  bool ProtocolEqualsSelf(const String&) const;
  const String& GetSelfProtocol() const;

  bool ExperimentalFeaturesEnabled() const;

  bool ShouldSendCSPHeader(ResourceType) const;

  CSPSource* GetSelfSource() const { return self_source_; }

  static bool ShouldBypassMainWorld(const ExecutionContext*);

  static bool IsNonceableElement(const Element*);

  // This method checks whether the request should be allowed for an
  // experimental EmbeddingCSP feature
  // Please, see https://w3c.github.io/webappsec-csp/embedded/#origin-allowed.
  static bool ShouldEnforceEmbeddersPolicy(const ResourceResponse&,
                                           const SecurityOrigin*);

  static const char* GetDirectiveName(const DirectiveType&);
  static DirectiveType GetDirectiveType(const String& name);

  // This method checks if if this policy subsumes a given policy.
  // Note the correct result is guaranteed if this policy contains only one
  // CSPDirectiveList. More information here:
  // https://w3c.github.io/webappsec-csp/embedded/#subsume-policy
  bool Subsumes(const ContentSecurityPolicy&) const;

  bool HasHeaderDeliveredPolicy() const { return header_delivered_; }

  static bool IsValidCSPAttr(const String& attr,
                             const String& context_required_csp);

  // Returns the 'wasm-eval' source is supported.
  bool SupportsWasmEval() const { return supports_wasm_eval_; }

  // Sometimes we don't know the initiator or it might be destroyed already
  // for certain navigational checks. We create a string version of the relevant
  // CSP directives to be passed around with the request. This allows us to
  // perform these checks in NavigationRequest::CheckContentSecurityPolicy.
  WebContentSecurityPolicyList ExposeForNavigationalChecks() const;

  // Retrieves the parsed sandbox flags. A lot of the time the execution
  // context will be used for all sandbox checks but there are situations
  // (before installing the document that this CSP will bind to) when
  // there is no execution context to enforce the sandbox flags.
  SandboxFlags GetSandboxMask() const { return sandbox_mask_; }

  bool HasPolicyFromSource(ContentSecurityPolicyHeaderSource) const;

  static bool IsScriptDirective(
      ContentSecurityPolicy::DirectiveType directive_type) {
    return (
        directive_type == ContentSecurityPolicy::DirectiveType::kScriptSrc ||
        directive_type ==
            ContentSecurityPolicy::DirectiveType::kScriptSrcAttr ||
        directive_type == ContentSecurityPolicy::DirectiveType::kScriptSrcElem);
  }

  static bool IsStyleDirective(
      ContentSecurityPolicy::DirectiveType directive_type) {
    return (
        directive_type == ContentSecurityPolicy::DirectiveType::kStyleSrc ||
        directive_type == ContentSecurityPolicy::DirectiveType::kStyleSrcAttr ||
        directive_type == ContentSecurityPolicy::DirectiveType::kStyleSrcElem);
  }

  void Count(WebFeature feature) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentSecurityPolicyTest, NonceInline);
  FRIEND_TEST_ALL_PREFIXES(ContentSecurityPolicyTest, NonceSinglePolicy);
  FRIEND_TEST_ALL_PREFIXES(ContentSecurityPolicyTest, NonceMultiplePolicy);
  FRIEND_TEST_ALL_PREFIXES(ContentSecurityPolicyTest, EmptyCSPIsNoOp);
  FRIEND_TEST_ALL_PREFIXES(BaseFetchContextTest, CanRequest);
  FRIEND_TEST_ALL_PREFIXES(BaseFetchContextTest, CheckCSPForRequest);
  FRIEND_TEST_ALL_PREFIXES(BaseFetchContextTest,
                           AllowResponseChecksReportedAndEnforcedCSP);
  FRIEND_TEST_ALL_PREFIXES(FrameFetchContextTest,
                           PopulateResourceRequestChecksReportOnlyCSP);

  void ApplyPolicySideEffectsToDelegate();

  void LogToConsole(
      const String& message,
      mojom::ConsoleMessageLevel = mojom::ConsoleMessageLevel::kError);

  void AddAndReportPolicyFromHeaderValue(const String&,
                                         ContentSecurityPolicyHeaderType,
                                         ContentSecurityPolicyHeaderSource);

  bool ShouldSendViolationReport(const String&) const;
  void DidSendViolationReport(const String&);
  void PostViolationReport(const SecurityPolicyViolationEventInit*,
                           LocalFrame*,
                           const Vector<String>& report_endpoints,
                           bool use_reporting_api);

  bool AllowFromSource(ContentSecurityPolicy::DirectiveType,
                       const KURL&,
                       RedirectStatus = RedirectStatus::kNoRedirect,
                       SecurityViolationReportingPolicy =
                           SecurityViolationReportingPolicy::kReport,
                       CheckHeaderType = CheckHeaderType::kCheckAll,
                       const String& = String(),
                       const IntegrityMetadataSet& = IntegrityMetadataSet(),
                       ParserDisposition = kParserInserted) const;

  static void FillInCSPHashValues(const String& source,
                                  uint8_t hash_algorithms_used,
                                  Vector<CSPHashValue>* csp_hash_values);

  // checks a vector of csp hashes against policy, probably a good idea
  // to use in tandem with FillInCSPHashValues.
  static bool CheckHashAgainstPolicy(Vector<CSPHashValue>&,
                                     const Member<CSPDirectiveList>&,
                                     InlineType);

  bool ShouldBypassContentSecurityPolicy(
      const KURL&,
      SchemeRegistry::PolicyAreas = SchemeRegistry::kPolicyAreaAll) const;

  Member<ContentSecurityPolicyDelegate> delegate_;
  bool override_inline_style_allowed_;
  CSPDirectiveListVector policies_;
  ConsoleMessageVector console_messages_;
  bool header_delivered_{false};

  HashSet<unsigned, AlreadyHashed> violation_reports_sent_;

  // We put the hash functions used on the policy object so that we only need
  // to calculate a hash once and then distribute it to all of the directives
  // for validation.
  uint8_t script_hash_algorithms_used_;
  uint8_t style_hash_algorithms_used_;

  // State flags used to configure the environment after parsing a policy.
  SandboxFlags sandbox_mask_;
  bool require_trusted_types_;
  String disable_eval_error_message_;
  WebInsecureRequestPolicy insecure_request_policy_;

  Member<CSPSource> self_source_;
  String self_protocol_;

  bool supports_wasm_eval_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CONTENT_SECURITY_POLICY_H_
