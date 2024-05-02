// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PERMISSIONS_POLICY_IFRAME_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PERMISSIONS_POLICY_IFRAME_POLICY_H_

#include "third_party/blink/renderer/core/permissions_policy/dom_feature_policy.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// IFramePolicy inherits Policy. It represents the permissions policy of an
// iframe contained in a document, as seen from that document (not including any
// information private to that frame). It is synthesized from the parent
// document's policy and the iframe's container policy.
class IFramePolicy final : public DOMFeaturePolicy {
 public:
  ~IFramePolicy() override = default;

  // Create a new IFramePolicy, which is synthetic for a frame contained within
  // a document.
  IFramePolicy(ExecutionContext* parent_context,
               const ParsedPermissionsPolicy& container_policy,
               scoped_refptr<const SecurityOrigin> src_origin)
      : DOMFeaturePolicy(parent_context) {
    DCHECK(src_origin);
    UpdateContainerPolicy(container_policy, src_origin);
  }

  void UpdateContainerPolicy(
      const ParsedPermissionsPolicy& container_policy,
      scoped_refptr<const SecurityOrigin> src_origin) override {
    policy_ = PermissionsPolicy::CreateFromParentPolicy(
        context_->GetSecurityContext().GetPermissionsPolicy(),
        /*header_policy=*/{}, container_policy, src_origin->ToUrlOrigin());
  }

 protected:
  const PermissionsPolicy* GetPolicy() const override { return policy_.get(); }

  bool IsIFramePolicy() const override { return true; }

 private:
  std::unique_ptr<PermissionsPolicy> policy_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PERMISSIONS_POLICY_IFRAME_POLICY_H_
