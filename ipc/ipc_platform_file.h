// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_PLATFORM_FILE_H_
#define IPC_IPC_PLATFORM_FILE_H_

#include "base/files/file.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "ipc/ipc_message_support_export.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/file_descriptor_posix.h"
#endif

namespace IPC {

#if BUILDFLAG(IS_WIN)
class IPC_MESSAGE_SUPPORT_EXPORT PlatformFileForTransit {
 public:
  // Creates an invalid platform file.
  PlatformFileForTransit();

  // Creates a platform file that takes unofficial ownership of |handle|. Note
  // that ownership is not handled by a Scoped* class due to usage patterns of
  // this class and its POSIX counterpart [base::FileDescriptor]. When this
  // class is used as an input to an IPC message, the IPC subsystem will close
  // |handle|. When this class is used as the output from an IPC message, the
  // receiver is expected to take ownership of |handle|.
  explicit PlatformFileForTransit(HANDLE handle);

  // Comparison operators.
  bool operator==(const PlatformFileForTransit& platform_file) const;
  bool operator!=(const PlatformFileForTransit& platform_file) const;

  HANDLE GetHandle() const;
  bool IsValid() const;

 private:
  HANDLE handle_;
};
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
typedef base::FileDescriptor PlatformFileForTransit;
#endif

inline PlatformFileForTransit InvalidPlatformFileForTransit() {
#if BUILDFLAG(IS_WIN)
  return PlatformFileForTransit();
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return base::FileDescriptor();
#endif
}

inline base::PlatformFile PlatformFileForTransitToPlatformFile(
    const PlatformFileForTransit& transit) {
#if BUILDFLAG(IS_WIN)
  return transit.GetHandle();
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return transit.fd;
#endif
}

inline base::File PlatformFileForTransitToFile(
    const PlatformFileForTransit& transit) {
#if BUILDFLAG(IS_WIN)
  return base::File(transit.GetHandle());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return base::File(transit.fd);
#endif
}

// Creates a new handle that can be passed through IPC. The result must be
// passed to the IPC layer as part of a message, or else it will leak.
IPC_MESSAGE_SUPPORT_EXPORT PlatformFileForTransit
GetPlatformFileForTransit(base::PlatformFile file, bool close_source_handle);

// Creates a new handle that can be passed through IPC. The result must be
// passed to the IPC layer as part of a message, or else it will leak.
// Note that this function takes ownership of |file|.
IPC_MESSAGE_SUPPORT_EXPORT PlatformFileForTransit
TakePlatformFileForTransit(base::File file);

}  // namespace IPC

#endif  // IPC_IPC_PLATFORM_FILE_H_
