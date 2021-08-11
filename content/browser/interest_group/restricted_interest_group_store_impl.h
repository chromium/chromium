// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_RESTRICTED_INTEREST_GROUP_STORE_IMPL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_RESTRICTED_INTEREST_GROUP_STORE_IMPL_H_

#include "content/browser/interest_group/interest_group_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/interest_group/restricted_interest_group_store.mojom.h"

namespace content {

class RenderFrameHost;

// Implements the RestrictedInterestGroupStore service called by Blink code.
class CONTENT_EXPORT RestrictedInterestGroupStoreImpl final
    : public DocumentServiceBase<blink::mojom::RestrictedInterestGroupStore> {
 public:
  // Factory method for creating an instance of this interface that is bound
  // to the lifetime of the frame or receiver (whichever is shorter).
  static void CreateMojoService(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::RestrictedInterestGroupStore>
          receiver);

  // blink::mojom::RestrictedInterestGroupStore.
  void JoinInterestGroup(const blink::InterestGroup& group) override;
  void LeaveInterestGroup(const url::Origin& owner,
                          const std::string& name) override;
  void UpdateAdInterestGroups() override;

 private:
  // `render_frame_host` must not be null, and DocumentServiceBase guarantees
  // `this` will not outlive the `render_frame_host`.
  RestrictedInterestGroupStoreImpl(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::RestrictedInterestGroupStore>
          receiver);

  // `this` can only be destroyed by DocumentServiceBase.
  ~RestrictedInterestGroupStoreImpl() override;

  InterestGroupManager& interest_group_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_RESTRICTED_INTEREST_GROUP_STORE_IMPL_H_
