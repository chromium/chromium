// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_layer_mailbox_manager.h"

#include "third_party/blink/renderer/modules/xr/xr_layer.h"

namespace blink {

void XRLayerMailboxManager::Reset() {
  layer_mailboxes_.clear();
}

void XRLayerMailboxManager::SetLayerMailboxes(
    XRLayer* layer,
    const std::optional<gpu::MailboxHolder>& color_mailbox_holder,
    const std::optional<gpu::MailboxHolder>& camera_image_mailbox_holder) {
  XRLayerMailboxes mailboxes = {color_mailbox_holder,
                                camera_image_mailbox_holder};
  layer_mailboxes_.Set(layer->layer_id(), mailboxes);
}

const XRLayerMailboxes& XRLayerMailboxManager::GetLayerMailboxes(
    const XRLayer* layer) const {
  auto mailboxes = layer_mailboxes_.find(layer->layer_id());
  if (mailboxes == layer_mailboxes_.end()) {
    return empty_mailboxes_;
  }

  return mailboxes->value;
}

}  // namespace blink
