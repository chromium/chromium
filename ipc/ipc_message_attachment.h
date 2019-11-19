// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_ATTACHMENT_H_
#define IPC_IPC_MESSAGE_ATTACHMENT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/pickle.h"
#include "build/build_config.h"
#include "ipc/ipc_message_support_export.h"
#include "mojo/public/cpp/system/handle.h"

namespace IPC {

// Auxiliary data sent with |Message|. This can be a platform file descriptor
// or a mojo |MessagePipe|. |GetType()| returns the type of the subclass.
class IPC_MESSAGE_SUPPORT_EXPORT MessageAttachment
    : public base::Pickle::Attachment {
 public:
  enum class Type {
    MOJO_HANDLE,
    PLATFORM_FILE,
    WIN_HANDLE,
    MACH_PORT,
    FUCHSIA_HANDLE,
  };

  static scoped_refptr<MessageAttachment> CreateFromMojoHandle(
      mojo::ScopedHandle handle,
      Type type);

  virtual Type GetType() const = 0;

  mojo::ScopedHandle TakeMojoHandle();

 protected:
  friend class base::RefCountedThreadSafe<MessageAttachment>;
  MessageAttachment();
  ~MessageAttachment() override;

  DISALLOW_COPY_AND_ASSIGN(MessageAttachment);
};

}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_ATTACHMENT_H_
