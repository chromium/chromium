// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_REMOTE_SECURITY_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_REMOTE_SECURITY_CONTEXT_H_

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CORE_EXPORT RemoteSecurityContext
    : public GarbageCollected<RemoteSecurityContext>,
      public SecurityContext {
  USING_GARBAGE_COLLECTED_MIXIN(RemoteSecurityContext);

 public:
  RemoteSecurityContext();

  void Trace(blink::Visitor*) override;

  void SetReplicatedOrigin(scoped_refptr<SecurityOrigin>);
  void ResetReplicatedContentSecurityPolicy();
  void ResetAndEnforceSandboxFlags(WebSandboxFlags flags);

  // Constructs the enforcement FeaturePolicy struct for this security context.
  // The resulting FeaturePolicy is a combination of:
  //   * |parsed_header|: from the FeaturePolicy part of the response headers.
  //   * |container_policy|: from <iframe>'s allow attribute.
  //   * |parent_feature_policy|: which is the current state of feature policies
  //     in a parent browsing context (frame).
  //   * |opener_feature_state|: the current state of the policies in an opener
  //     if any.
  // Note that at most one of the |parent_feature_policy| or
  // |opener_feature_state| should be provided. The |container_policy| is empty
  // for a top-level security context.
  void InitializeFeaturePolicy(
      const ParsedFeaturePolicy& parsed_header,
      const ParsedFeaturePolicy& container_policy,
      const FeaturePolicy* parent_feature_policy,
      const FeaturePolicy::FeatureState* opener_feature_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_REMOTE_SECURITY_CONTEXT_H_
