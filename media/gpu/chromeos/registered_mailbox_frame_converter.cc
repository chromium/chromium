// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/registered_mailbox_frame_converter.h"

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "media/gpu/chromeos/mailbox_frame_registry.h"
#include "media/gpu/macros.h"

namespace media {

// static
std::unique_ptr<FrameResourceConverter> RegisteredMailboxFrameConverter::Create(
    scoped_refptr<MailboxFrameRegistry> registry) {
  return base::WrapUnique<FrameResourceConverter>(
      new RegisteredMailboxFrameConverter(std::move(registry)));
}

RegisteredMailboxFrameConverter::RegisteredMailboxFrameConverter(
    scoped_refptr<MailboxFrameRegistry> registry)
    : registry_(std::move(registry)) {}

RegisteredMailboxFrameConverter::~RegisteredMailboxFrameConverter() = default;

void RegisteredMailboxFrameConverter::ConvertFrameImpl(
    scoped_refptr<FrameResource> frame) {
  DVLOGF(4);

  if (!frame) {
    return OnError(FROM_HERE, "Invalid frame.");
  }

  // A reference to |frame| is stored in |registry_|. Later, a reference to
  // |registry_| will be stored in the release mailbox callback for the
  // generated frame, |mailbox_frame|. So, the local reference to |frame| can be
  // safely dropped at the end of this function.
  const gpu::Mailbox mailbox = registry_->RegisterFrame(frame);

  // Creates a mailbox-backed VideoFrame with |mailbox| and |frame|'s metadata.
  scoped_refptr<VideoFrame> mailbox_frame = VideoFrame::WrapOOPVDMailbox(
      frame->format(), mailbox, VideoFrame::ReleaseMailboxCB(),
      frame->coded_size(), frame->visible_rect(), frame->natural_size(),
      frame->timestamp());
  if (!mailbox_frame) {
    // Removes the reference to |frame| from |registry_|.
    registry_->UnregisterFrame(mailbox);
    return OnError(FROM_HERE, "Failed to create a mailbox frame.");
  }

  mailbox_frame->set_color_space(frame->ColorSpace());
  mailbox_frame->set_hdr_metadata(frame->hdr_metadata());
  mailbox_frame->set_metadata(frame->metadata());
  // Adds a destruction observer such that when |mailbox_frame| is destroyed,
  // the reference to |frame| is dropped from |registry_|. This lets it be
  // returned the decoder's frame pool.
  mailbox_frame->AddDestructionObserver(base::BindOnce(
      [](scoped_refptr<MailboxFrameRegistry> registry,
         const gpu::Mailbox& mailbox) { registry->UnregisterFrame(mailbox); },
      registry_, mailbox));

  Output(std::move(mailbox_frame));
}

}  // namespace media
