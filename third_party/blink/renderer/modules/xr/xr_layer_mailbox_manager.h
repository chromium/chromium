// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_MAILBOX_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_MAILBOX_MANAGER_H_

#include <optional>

#include "gpu/command_buffer/common/mailbox_holder.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class XRLayer;

struct XRLayerMailboxes {
  std::optional<gpu::MailboxHolder> color_mailbox_holder;
  std::optional<gpu::MailboxHolder> camera_image_mailbox_holder;
};

class XRLayerMailboxManager {
 public:
  XRLayerMailboxManager() = default;
  ~XRLayerMailboxManager() = default;

  void Reset();
  void SetLayerMailboxes(
      XRLayer*,
      const std::optional<gpu::MailboxHolder>& color_mailbox_holder,
      const std::optional<gpu::MailboxHolder>& camera_image_mailbox_holder);

  const XRLayerMailboxes& GetLayerMailboxes(const XRLayer*) const;

 private:
  XRLayerMailboxes empty_mailboxes_;
  WTF::HashMap<uint32_t, XRLayerMailboxes> layer_mailboxes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_MAILBOX_MANAGER_H_
