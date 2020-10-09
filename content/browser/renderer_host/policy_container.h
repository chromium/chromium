// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_H_
#define CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_H_

#include "content/common/content_export.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"

namespace content {

// PolicyContainer serves as a container for several security policies. It
// should be owned by a RenderFrameHost. It keep tracks of the policies assigned
// to a document. When a document creates/opens another document with a local
// scheme (about:blank, about:srcdoc, data, blob, filesystem), the policy
// container of the opener is cloned and a copy is attached to the new document,
// so that the same security policies are applied to it.
class CONTENT_EXPORT PolicyContainer {
 public:
  PolicyContainer();
  explicit PolicyContainer(network::mojom::ReferrerPolicy referrer_policy);
  PolicyContainer(const PolicyContainer&) = delete;
  PolicyContainer& operator=(const PolicyContainer&) = delete;

  void SetReferrerPolicy(network::mojom::ReferrerPolicy referrer_policy);
  network::mojom::ReferrerPolicy referrer_policy() const {
    return referrer_policy_;
  }

  std::unique_ptr<PolicyContainer> Clone() const;

 private:
  // The referrer policy for the associated document. If not overwritten via a
  // call to SetReferrerPolicy (for example after parsing the Referrer-Policy
  // header or a meta tag), the default referrer policy will be applied to the
  // document.
  network::mojom::ReferrerPolicy referrer_policy_ =
      network::mojom::ReferrerPolicy::kDefault;
};

}  // namespace content

#endif
