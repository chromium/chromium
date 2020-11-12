// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_HOST_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom.h"

namespace content {

// PolicyContainerHost serves as a container for several security policies. It
// should be owned by a RenderFrameHost. It keep tracks of the policies assigned
// to a document. When a document creates/opens another document with a local
// scheme (about:blank, about:srcdoc, data, blob, filesystem), the
// PolicyContainerHost of the opener is cloned and a copy is attached to the new
// document, so that the same security policies are applied to it. It implements
// a mojo interface that allows updates coming from Blink.
class CONTENT_EXPORT PolicyContainerHost
    : public blink::mojom::PolicyContainerHost {
 public:
  struct DocumentPolicies {
    // The referrer policy for the associated document. If not overwritten via a
    // call to SetReferrerPolicy (for example after parsing the Referrer-Policy
    // header or a meta tag), the default referrer policy will be applied to the
    // document.
    network::mojom::ReferrerPolicy referrer_policy =
        network::mojom::ReferrerPolicy::kDefault;
  };

  PolicyContainerHost();
  explicit PolicyContainerHost(DocumentPolicies document_policies);
  PolicyContainerHost(const PolicyContainerHost&) = delete;
  PolicyContainerHost& operator=(const PolicyContainerHost&) = delete;
  ~PolicyContainerHost() override;

  network::mojom::ReferrerPolicy referrer_policy() const {
    return document_policies_.referrer_policy;
  }

  const DocumentPolicies& document_policies() const {
    return document_policies_;
  }

  // Return a PolicyContainer containing copies of the policies and a pending
  // mojo remote that can be used to update policies in this object. If called a
  // second time, it resets the receiver and creates a new PolicyContainer,
  // invalidating the remote of the previous one.
  blink::mojom::PolicyContainerPtr CreatePolicyContainerForBlink();

  // Create a new PolicyContainerHost with the same policies (i.e. a deep copy),
  // but with a new, unbound mojo receiver.
  std::unique_ptr<PolicyContainerHost> Clone() const;

  // Bind this PolicyContainerHost with the given mojo receiver, so that it can
  // handle mojo messages coming from the corresponding remote.
  void Bind(mojo::PendingAssociatedReceiver<blink::mojom::PolicyContainerHost>
                receiver);

 private:
  void SetReferrerPolicy(network::mojom::ReferrerPolicy referrer_policy) final;

  DocumentPolicies document_policies_;

  mojo::AssociatedReceiver<blink::mojom::PolicyContainerHost>
      policy_container_host_receiver_{this};
};

}  // namespace content

#endif
