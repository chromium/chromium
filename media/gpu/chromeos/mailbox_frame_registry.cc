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
  base::AutoLock auto_lock(lock_);
  CHECK_LT(mailbox_id_counter_, std::numeric_limits<uint64_t>::max());
  ++mailbox_id_counter_;
  static_assert(sizeof(mailbox_id_counter_) <= sizeof(name));
  memcpy(name, &mailbox_id_counter_, sizeof(mailbox_id_counter_));
  mailbox.SetName(name);
  CHECK(!mailbox.IsZero());
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
