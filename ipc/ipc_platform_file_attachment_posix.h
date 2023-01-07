// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_PLATFORM_FILE_ATTACHMENT_POSIX_H_
#define IPC_IPC_PLATFORM_FILE_ATTACHMENT_POSIX_H_

#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"
#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_message_support_export.h"

namespace IPC {
namespace internal {

// A platform file that is sent over |Channel| as a part of |Message|.
// PlatformFileAttachment optionally owns the file and |owning_| is set in that
// case. Also, |file_| is not cleared even after the ownership is taken.
// Some old clients require this strange behavior.
class IPC_MESSAGE_SUPPORT_EXPORT PlatformFileAttachment
    : public MessageAttachment {
 public:
  // Non-owning constructor
  explicit PlatformFileAttachment(base::PlatformFile file);
  // Owning constructor
  explicit PlatformFileAttachment(base::ScopedFD file);

  Type GetType() const override;
  base::PlatformFile TakePlatformFile();

  base::PlatformFile file() const { return file_; }
  bool Owns() const { return owning_.is_valid(); }

 private:
  ~PlatformFileAttachment() override;

  base::PlatformFile file_;
  base::ScopedFD owning_;
};

base::PlatformFile GetPlatformFile(scoped_refptr<MessageAttachment> attachment);

}  // namespace internal
}  // namespace IPC

#endif  // IPC_IPC_PLATFORM_FILE_ATTACHMENT_POSIX_H_
