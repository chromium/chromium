// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_IFRAME_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_IFRAME_POLICY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/feature_policy/policy.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// IFramePolicy inherits Policy. It represents the feature policy introspection
// of an iframe contained in a document. It is tynthetic from the parent policy
// and the iframe container policy (parsed from the allow attribute).
class IFramePolicy final : public Policy {
 public:
  ~IFramePolicy() override = default;

  // Create a new IFramePolicy, which is synthetic for a frame contained within
  // a document.
  IFramePolicy(Document* parent_document,
               const ParsedFeaturePolicy& container_policy,
               scoped_refptr<const SecurityOrigin> src_origin)
      : parent_document_(parent_document) {
    DCHECK(src_origin);
    UpdateContainerPolicy(container_policy, src_origin);
  }

  void UpdateContainerPolicy(
      const ParsedFeaturePolicy& container_policy,
      scoped_refptr<const SecurityOrigin> src_origin) override {
    policy_ = FeaturePolicy::CreateFromParentPolicy(
        parent_document_->GetFeaturePolicy(), container_policy,
        src_origin->ToUrlOrigin());
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(parent_document_);
    Policy::Trace(visitor);
  }

 protected:
  const FeaturePolicy* GetPolicy() const override { return policy_.get(); }
  Document* GetDocument() const override { return parent_document_; }

 private:
  Member<Document> parent_document_;
  std::unique_ptr<FeaturePolicy> policy_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_IFRAME_POLICY_H_
