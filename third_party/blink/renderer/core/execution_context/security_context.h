/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_SECURITY_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_SECURITY_CONTEXT_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/sandbox_flags.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

#include <memory>

namespace blink {

class ContentSecurityPolicy;
class FeaturePolicy;
class SecurityOrigin;
struct ParsedFeaturePolicyDeclaration;

using ParsedFeaturePolicy = std::vector<ParsedFeaturePolicyDeclaration>;

// Whether to report policy violations when checking whether a feature is
// enabled.
enum class ReportOptions { kReportOnFailure, kDoNotReport };

namespace mojom {
enum class FeaturePolicyFeature : int32_t;
enum class IPAddressSpace : int32_t;
}

// Defines the security properties (such as the security origin, content
// security policy, and other restrictions) of an environment in which
// script execution or other activity may occur.
//
// Mostly 1:1 with ExecutionContext, except that while remote (i.e.,
// out-of-process) environments do not have an ExecutionContext in the local
// process (as execution cannot occur locally), they do have a SecurityContext
// to allow those properties to be queried.
class CORE_EXPORT SecurityContext : public GarbageCollectedMixin {
 public:
  void Trace(blink::Visitor*) override;

  using InsecureNavigationsSet = HashSet<unsigned, WTF::AlreadyHashed>;
  static std::vector<unsigned> SerializeInsecureNavigationSet(
      const InsecureNavigationsSet&);

  const SecurityOrigin* GetSecurityOrigin() const {
    return security_origin_.get();
  }
  SecurityOrigin* GetMutableSecurityOrigin() { return security_origin_.get(); }

  ContentSecurityPolicy* GetContentSecurityPolicy() const {
    return content_security_policy_.Get();
  }

  // Explicitly override the security origin for this security context.
  // Note: It is dangerous to change the security origin of a script context
  //       that already contains content.
  void SetSecurityOrigin(scoped_refptr<SecurityOrigin>);
  virtual void DidUpdateSecurityOrigin() = 0;

  SandboxFlags GetSandboxFlags() const { return sandbox_flags_; }
  bool IsSandboxed(SandboxFlags mask) const {
    return IsSandboxed(mask, sandbox_flags_);
  }
  static bool IsSandboxed(SandboxFlags mask, SandboxFlags sandbox_flags) {
    return sandbox_flags & mask;
  }
  virtual void EnforceSandboxFlags(SandboxFlags mask);

  void SetAddressSpace(mojom::IPAddressSpace space) { address_space_ = space; }
  mojom::IPAddressSpace AddressSpace() const { return address_space_; }
  String addressSpaceForBindings() const;

  void SetRequireTrustedTypes() { require_safe_types_ = true; }
  bool RequireTrustedTypes() const { return require_safe_types_; }

  void SetInsecureNavigationsSet(const std::vector<unsigned>& set) {
    insecure_navigations_to_upgrade_.clear();
    for (unsigned hash : set)
      insecure_navigations_to_upgrade_.insert(hash);
  }
  void AddInsecureNavigationUpgrade(unsigned hashed_host) {
    insecure_navigations_to_upgrade_.insert(hashed_host);
  }
  InsecureNavigationsSet* InsecureNavigationsToUpgrade() {
    return &insecure_navigations_to_upgrade_;
  }

  virtual void SetInsecureRequestPolicy(WebInsecureRequestPolicy policy) {
    insecure_request_policy_ = policy;
  }
  WebInsecureRequestPolicy GetInsecureRequestPolicy() const {
    return insecure_request_policy_;
  }

  FeaturePolicy* GetFeaturePolicy() const { return feature_policy_.get(); }
  void SetFeaturePolicy(std::unique_ptr<FeaturePolicy> feature_policy);
  void InitializeFeaturePolicy(const ParsedFeaturePolicy& parsed_header,
                               const ParsedFeaturePolicy& container_policy,
                               const FeaturePolicy* parent_feature_policy);

  // Tests whether the policy-controlled feature is enabled in this frame.
  // Optionally sends a report to any registered reporting observers or
  // Report-To endpoints, via ReportFeaturePolicyViolation(), if the feature is
  // disabled. The optional ConsoleMessage will be sent to the console if
  // present, or else a default message will be used instead.
  bool IsFeatureEnabled(
      mojom::FeaturePolicyFeature,
      ReportOptions report_on_failure = ReportOptions::kDoNotReport,
      const String& message = g_empty_string) const;
  virtual void ReportFeaturePolicyViolation(
      mojom::FeaturePolicyFeature,
      const String& message = g_empty_string) const {}

  // Apply the sandbox flag. In addition, if the origin is not already opaque,
  // the origin is updated to a newly created unique opaque origin, setting the
  // potentially trustworthy bit from |is_potentially_trustworthy|.
  void ApplySandboxFlags(SandboxFlags mask,
                         bool is_potentially_trustworthy = false);

 protected:
  SecurityContext();
  virtual ~SecurityContext();

  void SetContentSecurityPolicy(ContentSecurityPolicy*);

  SandboxFlags sandbox_flags_;

 private:
  scoped_refptr<SecurityOrigin> security_origin_;
  Member<ContentSecurityPolicy> content_security_policy_;
  std::unique_ptr<FeaturePolicy> feature_policy_;

  mojom::IPAddressSpace address_space_;
  WebInsecureRequestPolicy insecure_request_policy_;
  InsecureNavigationsSet insecure_navigations_to_upgrade_;
  bool require_safe_types_;
  DISALLOW_COPY_AND_ASSIGN(SecurityContext);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_SECURITY_CONTEXT_H_
