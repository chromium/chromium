// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_SECURITY_CONTEXT_INIT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_SECURITY_CONTEXT_INIT_H_

#include "third_party/blink/public/common/feature_policy/document_policy.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink-forward.h"
#include "third_party/blink/public/web/web_origin_policy.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/feature_policy/policy_helper.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class LocalFrame;
class ResourceResponse;

class CORE_EXPORT SecurityContextInit {
  STACK_ALLOCATED();

 public:
  explicit SecurityContextInit(ExecutionContext*);

  // Init |feature_policy_| and |report_only_feature_policy_| by copying
  // state from another security context instance.
  // Used to carry feature policy information from previous document
  // to current document during XSLT navigation, because XSLT navigation
  // does not have header information available.
  void InitFeaturePolicyFrom(const SecurityContext& other);

  // Init |document_policy_| and |report_only_document_policy_| by copying
  // state from another security context instance.
  // Used to carry document policy information from previous document
  // to current document during XSLT navigation, because XSLT navigation
  // does not have header information available.
  void InitDocumentPolicyFrom(const SecurityContext& other);

  void ApplyFeaturePolicy(LocalFrame* frame,
                          const ResourceResponse& response,
                          const base::Optional<WebOriginPolicy>& origin_policy,
                          const FramePolicy& frame_policy);
  void ApplyDocumentPolicy(
      DocumentPolicy::ParsedDocumentPolicy& document_policy,
      const String& report_only_document_policy_header);

  const ParsedFeaturePolicy& FeaturePolicyHeader() const {
    return feature_policy_header_;
  }

 private:
  ExecutionContext* execution_context_ = nullptr;
  ParsedFeaturePolicy feature_policy_header_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_SECURITY_CONTEXT_INIT_H_
