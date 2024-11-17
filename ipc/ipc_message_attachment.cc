// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_attachment.h"

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "ipc/ipc_mojo_handle_attachment.h"
#include "mojo/public/cpp/system/platform_handle.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "ipc/mach_port_attachment_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ipc/handle_attachment_win.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "ipc/handle_attachment_fuchsia.h"
#endif

namespace IPC {

namespace {

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
base::ScopedFD TakeOrDupFile(internal::PlatformFileAttachment* attachment) {
  return attachment->Owns()
             ? base::ScopedFD(attachment->TakePlatformFile())
             : base::ScopedFD(HANDLE_EINTR(dup(attachment->file())));
}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

}  // namespace

MessageAttachment::MessageAttachment() = default;

MessageAttachment::~MessageAttachment() = default;

mojo::ScopedHandle MessageAttachment::TakeMojoHandle() {
  switch (GetType()) {
    case Type::MOJO_HANDLE:
      return static_cast<internal::MojoHandleAttachment*>(this)->TakeHandle();

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    case Type::PLATFORM_FILE: {
      // We dup() the handles in IPC::Message to transmit.
      // IPC::MessageAttachmentSet has intricate lifetime semantics for FDs, so
      // just to dup()-and-own them is the safest option.
      base::ScopedFD file =
          TakeOrDupFile(static_cast<internal::PlatformFileAttachment*>(this));
      if (!file.is_valid()) {
        DPLOG(WARNING) << "Failed to dup FD to transmit.";
        return mojo::ScopedHandle();
      }
      return mojo::WrapPlatformFile(std::move(file));
    }
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_MAC)
    case Type::MACH_PORT: {
      auto* attachment = static_cast<internal::MachPortAttachmentMac*>(this);
      MojoPlatformHandle platform_handle = {
          sizeof(platform_handle), MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT,
          static_cast<uint64_t>(attachment->get_mach_port())};
      MojoHandle wrapped_handle;
      if (MojoWrapPlatformHandle(&platform_handle, nullptr, &wrapped_handle) !=
          MOJO_RESULT_OK) {
        return mojo::ScopedHandle();
      }
      attachment->reset_mach_port_ownership();
      return mojo::MakeScopedHandle(mojo::Handle(wrapped_handle));
    }
#elif BUILDFLAG(IS_FUCHSIA)
    case Type::FUCHSIA_HANDLE: {
      auto* attachment = static_cast<internal::HandleAttachmentFuchsia*>(this);
      MojoPlatformHandle platform_handle = {
          sizeof(platform_handle), MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE,
          static_cast<uint64_t>(attachment->Take())};
      MojoHandle wrapped_handle;
      if (MojoWrapPlatformHandle(&platform_handle, nullptr, &wrapped_handle) !=
          MOJO_RESULT_OK) {
        return mojo::ScopedHandle();
      }
      return mojo::MakeScopedHandle(mojo::Handle(wrapped_handle));
    }
#elif BUILDFLAG(IS_WIN)
    case Type::WIN_HANDLE:
      return mojo::WrapPlatformFile(base::win::ScopedHandle(
          static_cast<internal::HandleAttachmentWin*>(this)->Take()));
#endif
    default:
      break;
  }
  NOTREACHED();
}

// static
scoped_refptr<MessageAttachment> MessageAttachment::CreateFromMojoHandle(
    mojo::ScopedHandle handle,
    Type type) {
  if (type == Type::MOJO_HANDLE)
    return new internal::MojoHandleAttachment(std::move(handle));

  MojoPlatformHandle platform_handle = {sizeof(platform_handle), 0, 0};
  MojoResult unwrap_result = MojoUnwrapPlatformHandle(
      handle.release().value(), nullptr, &platform_handle);
  if (unwrap_result != MOJO_RESULT_OK)
    return nullptr;

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  if (type == Type::PLATFORM_FILE) {
    base::PlatformFile file = base::kInvalidPlatformFile;
    if (platform_handle.type == MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR)
      file = static_cast<base::PlatformFile>(platform_handle.value);
    return new internal::PlatformFileAttachment(file);
  }
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_MAC)
  if (type == Type::MACH_PORT) {
    mach_port_t mach_port = MACH_PORT_NULL;
    if (platform_handle.type == MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT)
      mach_port = static_cast<mach_port_t>(platform_handle.value);
    return new internal::MachPortAttachmentMac(
        mach_port, internal::MachPortAttachmentMac::FROM_WIRE);
  }
#elif BUILDFLAG(IS_FUCHSIA)
  if (type == Type::FUCHSIA_HANDLE) {
    zx::handle zx_handle;
    if (platform_handle.type == MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE)
      zx_handle.reset(static_cast<zx_handle_t>(platform_handle.value));
    return new internal::HandleAttachmentFuchsia(std::move(zx_handle));
  }
#elif BUILDFLAG(IS_WIN)
  if (type == Type::WIN_HANDLE) {
    base::PlatformFile platform_file = base::kInvalidPlatformFile;
    if (platform_handle.type == MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE) {
      platform_file =
          reinterpret_cast<base::PlatformFile>(platform_handle.value);
    }
    return new internal::HandleAttachmentWin(
        platform_file, internal::HandleAttachmentWin::FROM_WIRE);
  }
#endif
  NOTREACHED();
}

}  // namespace IPC
