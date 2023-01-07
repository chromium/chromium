// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_errors.h"

#include <errno.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/safe_strerror.h"
#include "build/build_config.h"

namespace net {

Error MapSystemError(logging::SystemErrorCode os_error) {
  if (os_error != 0)
    DVLOG(2) << "Error " << os_error << ": "
             << logging::SystemErrorCodeToString(os_error);

  // There are numerous posix error codes, but these are the ones we thus far
  // find interesting.
  switch (os_error) {
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
      return ERR_IO_PENDING;
    case EACCES:
      return ERR_ACCESS_DENIED;
    case ENETDOWN:
      return ERR_INTERNET_DISCONNECTED;
    case ETIMEDOUT:
      return ERR_TIMED_OUT;
    case ECONNRESET:
    case ENETRESET:  // Related to keep-alive.
    case EPIPE:
      return ERR_CONNECTION_RESET;
    case ECONNABORTED:
      return ERR_CONNECTION_ABORTED;
    case ECONNREFUSED:
      return ERR_CONNECTION_REFUSED;
    case EHOSTUNREACH:
    case EHOSTDOWN:
    case ENETUNREACH:
    case EAFNOSUPPORT:
      return ERR_ADDRESS_UNREACHABLE;
    case EADDRNOTAVAIL:
      return ERR_ADDRESS_INVALID;
    case EMSGSIZE:
      return ERR_MSG_TOO_BIG;
    case ENOTCONN:
      return ERR_SOCKET_NOT_CONNECTED;
    case EISCONN:
      return ERR_SOCKET_IS_CONNECTED;
    case EINVAL:
      return ERR_INVALID_ARGUMENT;
    case EADDRINUSE:
      return ERR_ADDRESS_IN_USE;
    case E2BIG:  // Argument list too long.
      return ERR_INVALID_ARGUMENT;
    case EBADF:  // Bad file descriptor.
      return ERR_INVALID_HANDLE;
    case EBUSY:  // Device or resource busy.
      return ERR_INSUFFICIENT_RESOURCES;
    case ECANCELED:  // Operation canceled.
      return ERR_ABORTED;
    case EDEADLK:  // Resource deadlock avoided.
      return ERR_INSUFFICIENT_RESOURCES;
    case EDQUOT:  // Disk quota exceeded.
      return ERR_FILE_NO_SPACE;
    case EEXIST:  // File exists.
      return ERR_FILE_EXISTS;
    case EFAULT:  // Bad address.
      return ERR_INVALID_ARGUMENT;
    case EFBIG:  // File too large.
      return ERR_FILE_TOO_BIG;
    case EISDIR:  // Operation not allowed for a directory.
      return ERR_ACCESS_DENIED;
    case ENAMETOOLONG:  // Filename too long.
      return ERR_FILE_PATH_TOO_LONG;
    case ENFILE:  // Too many open files in system.
      return ERR_INSUFFICIENT_RESOURCES;
    case ENOBUFS:  // No buffer space available.
      return ERR_NO_BUFFER_SPACE;
    case ENODEV:  // No such device.
      return ERR_INVALID_ARGUMENT;
    case ENOENT:  // No such file or directory.
      return ERR_FILE_NOT_FOUND;
    case ENOLCK:  // No locks available.
      return ERR_INSUFFICIENT_RESOURCES;
    case ENOMEM:  // Not enough space.
      return ERR_OUT_OF_MEMORY;
    case ENOSPC:  // No space left on device.
      return ERR_FILE_NO_SPACE;
    case ENOSYS:  // Function not implemented.
      return ERR_NOT_IMPLEMENTED;
    case ENOTDIR:  // Not a directory.
      return ERR_FILE_NOT_FOUND;
    case ENOTSUP:  // Operation not supported.
      return ERR_NOT_IMPLEMENTED;
    case EPERM:  // Operation not permitted.
      return ERR_ACCESS_DENIED;
    case EROFS:  // Read-only file system.
      return ERR_ACCESS_DENIED;
    case ETXTBSY:  // Text file busy.
      return ERR_ACCESS_DENIED;
    case EUSERS:  // Too many users.
      return ERR_INSUFFICIENT_RESOURCES;
    case EMFILE:  // Too many open files.
      return ERR_INSUFFICIENT_RESOURCES;
    case ENOPROTOOPT:  // Protocol option not supported.
      return ERR_NOT_IMPLEMENTED;
#if BUILDFLAG(IS_FUCHSIA)
    case EIO:
      // FDIO maps all unrecognized errors to EIO. If you see this message then
      // consider adding custom error in FDIO for the corresponding error.
      DLOG(FATAL) << "EIO was returned by FDIO.";
      return ERR_FAILED;
#endif  // BUILDFLAG(IS_FUCHSIA)

    case 0:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << base::safe_strerror(os_error) << " ("
                   << os_error << ") mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

}  // namespace net
