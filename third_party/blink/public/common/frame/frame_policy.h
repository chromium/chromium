// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_POLICY_H_

#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/permissions_policy/document_policy_features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-shared.h"

namespace blink {

// This structure contains the attributes of a frame which determine what
// features are available during the lifetime of the framed document. Currently,
// this includes the sandbox flags, the permissions policy container policy, and
// document policy required policy. Used in the frame tree to track sandbox,
// permissions policy and document policy in the browser process, and
// transferred over IPC during frame replication when site isolation is enabled.
//
// Unlike the attributes in FrameOwnerProperties, these attributes are never
// updated after the framed document has been loaded, so two versions of this
// structure are kept in the frame tree for each frame -- the effective policy
// and the pending policy, which will take effect when the frame is next
// navigated.
struct BLINK_COMMON_EXPORT FramePolicy {
  // `DeferredFetchPolicy` tells how the deferred fetching feature is
  // enabled for the subframe of an owner frame. On navigation, a value should
  // be calculated by using the combination of the frame's inherited permissions
  // policies of "deferred-fetch" and "deferred-fetch-minimal".
  //
  // See https://whatpr.org/fetch/1647.html#deferred-fetch-policy for policy
  // definition and
  // https://whatpr.org/fetch/1647.html#determine-subframe-deferred-fetch-policy
  // for how to choose a value.
  enum class DeferredFetchPolicy {
    kDisabled,
    kDeferredFetch,
    kDeferredFetchMinimal,
  };

  FramePolicy();
  FramePolicy(network::mojom::WebSandboxFlags sandbox_flags,
              const ParsedPermissionsPolicy& container_policy,
              const DocumentPolicyFeatureState& required_document_policy,
              DeferredFetchPolicy deferred_fetch_policy);
  FramePolicy(const FramePolicy& lhs);
  ~FramePolicy();

  friend bool BLINK_COMMON_EXPORT operator==(const FramePolicy& lhs,
                                             const FramePolicy& rhs);

  network::mojom::WebSandboxFlags sandbox_flags;
  ParsedPermissionsPolicy container_policy;
  // |required_document_policy| is the combination of the following:
  // - iframe 'policy' attribute
  // - 'Require-Document-Policy' http header
  // - |required_document_policy| of parent frame
  DocumentPolicyFeatureState required_document_policy;
  // Derived from `container_policy` of the frame and the ancestor frames.
  DeferredFetchPolicy deferred_fetch_policy;
};

bool BLINK_COMMON_EXPORT operator!=(const FramePolicy& lhs,
                                    const FramePolicy& rhs);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_POLICY_H_
