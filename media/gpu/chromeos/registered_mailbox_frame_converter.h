// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_REGISTERED_MAILBOX_FRAME_CONVERTER_H_
#define MEDIA_GPU_CHROMEOS_REGISTERED_MAILBOX_FRAME_CONVERTER_H_

#include "base/memory/scoped_refptr.h"
#include "media/gpu/chromeos/frame_resource_converter.h"

namespace media {

class MailboxFrameRegistry;

// This class is used for converting FrameResources to gpu::Mailbox-backed
// FrameResources. It is constructed with a MailboxFrameRegistry which it uses
// to register frames that it converts. The frame's gpu::Mailbox can be used as
// a handle to access the original frame in the MailboxFrameRegistry. The
// MailboxFrameRegistry retains a reference to the FrameResource passed to
// ConvertFrame() until the returned frame is destroyed.
class RegisteredMailboxFrameConverter : public FrameResourceConverter {
 public:
  static std::unique_ptr<FrameResourceConverter> Create(
      scoped_refptr<MailboxFrameRegistry> registry);

  // RegisteredMailboxFrameConverter is not moveable or copyable.
  RegisteredMailboxFrameConverter(const RegisteredMailboxFrameConverter&) =
      delete;
  RegisteredMailboxFrameConverter& operator=(
      const RegisteredMailboxFrameConverter&) = delete;

 private:
  explicit RegisteredMailboxFrameConverter(
      scoped_refptr<MailboxFrameRegistry> registry);
  ~RegisteredMailboxFrameConverter() override;

  // FrameConverter overrides
  void ConvertFrameImpl(scoped_refptr<FrameResource> frame) override;

  // A frame registry, indexed by Mailbox. A reference to the original frame is
  // taken until the generated frame's release mailbox CB is called.
  scoped_refptr<MailboxFrameRegistry> registry_;
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_REGISTERED_MAILBOX_FRAME_CONVERTER_H_
