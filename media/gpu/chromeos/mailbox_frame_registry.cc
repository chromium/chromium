// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/mailbox_frame_registry.h"

#include "base/containers/contains.h"

namespace media {

MailboxFrameRegistry::MailboxFrameRegistry() = default;
MailboxFrameRegistry::~MailboxFrameRegistry() = default;

gpu::Mailbox MailboxFrameRegistry::RegisterFrame(
    scoped_refptr<const FrameResource> frame) {
  gpu::Mailbox mailbox;
  gpu::Mailbox::Name name{};
  // |unique_frame_id| is used to name |mailbox|. Using Mailbox::Generate()
  // creates a crytographically secure ID, but MailboxFrameRegistry just uses
  // the mailbox as an identifier. Furthermore, VideoDecoderPipeline generates a
  // Mailbox for each frame that is output. Instead of using
  // gpu::Mailbox::Generate(), this sets |mailbox|'s name from a simple counter.
  // |unique_frame_id| is unique within a process % overflows (which should be
  // impossible in practice with a 64-bit unsigned integer).
  static uint64_t unique_frame_id = 1;
  base::AutoLock auto_lock(lock_);
  ++unique_frame_id;
  memcpy(name, &unique_frame_id, sizeof(unique_frame_id));
  mailbox.SetName(name);
  map_.emplace(mailbox, std::move(frame));
  return mailbox;
}

void MailboxFrameRegistry::UnregisterFrame(const gpu::Mailbox& mailbox) {
  base::AutoLock auto_lock(lock_);
  CHECK_EQ(map_.erase(mailbox), 1u);
}

scoped_refptr<const FrameResource> MailboxFrameRegistry::AccessFrame(
    const gpu::Mailbox& mailbox) const {
  base::AutoLock auto_lock(lock_);
  // Ensures that |mailbox| exists in |map_|.
  auto it = map_.find(mailbox);
  CHECK(it != map_.end());
  return it->second;
}

}  // namespace media
