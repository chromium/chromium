// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_errors.h"

#include <winsock2.h>

#include "base/logging.h"

namespace net {

// Map winsock and system errors to Chromium errors.
Error MapSystemError(logging::SystemErrorCode os_error) {
  if (os_error != 0)
    DVLOG(2) << "Error " << os_error;

  // There are numerous Winsock error codes, but these are the ones we thus far
  // find interesting.
  switch (os_error) {
    case WSAEWOULDBLOCK:
    case WSA_IO_PENDING:
      return ERR_IO_PENDING;
    case WSAEACCES:
      return ERR_ACCESS_DENIED;
    case WSAENETDOWN:
      return ERR_INTERNET_DISCONNECTED;
    case WSAETIMEDOUT:
      return ERR_TIMED_OUT;
    case WSAECONNRESET:
    case WSAENETRESET:  // Related to keep-alive
      return ERR_CONNECTION_RESET;
    case WSAECONNABORTED:
      return ERR_CONNECTION_ABORTED;
    case WSAECONNREFUSED:
      return ERR_CONNECTION_REFUSED;
    case WSA_IO_INCOMPLETE:
    case WSAEDISCON:
      return ERR_CONNECTION_CLOSED;
    case WSAEISCONN:
      return ERR_SOCKET_IS_CONNECTED;
    case WSAEHOSTUNREACH:
    case WSAENETUNREACH:
      return ERR_ADDRESS_UNREACHABLE;
    case WSAEADDRNOTAVAIL:
      return ERR_ADDRESS_INVALID;
    case WSAEMSGSIZE:
      return ERR_MSG_TOO_BIG;
    case WSAENOTCONN:
      return ERR_SOCKET_NOT_CONNECTED;
    case WSAEAFNOSUPPORT:
      return ERR_ADDRESS_UNREACHABLE;
    case WSAEINVAL:
      return ERR_INVALID_ARGUMENT;
    case WSAEADDRINUSE:
      return ERR_ADDRESS_IN_USE;

    // System errors.
    case ERROR_FILE_NOT_FOUND:  // The system cannot find the file specified.
      return ERR_FILE_NOT_FOUND;
    case ERROR_PATH_NOT_FOUND:  // The system cannot find the path specified.
      return ERR_FILE_NOT_FOUND;
    case ERROR_TOO_MANY_OPEN_FILES:  // The system cannot open the file.
      return ERR_INSUFFICIENT_RESOURCES;
    case ERROR_ACCESS_DENIED:  // Access is denied.
      return ERR_ACCESS_DENIED;
    case ERROR_INVALID_HANDLE:  // The handle is invalid.
      return ERR_INVALID_HANDLE;
    case ERROR_NOT_ENOUGH_MEMORY:  // Not enough storage is available to
      return ERR_OUT_OF_MEMORY;    // process this command.
    case ERROR_OUTOFMEMORY:      // Not enough storage is available to complete
      return ERR_OUT_OF_MEMORY;  // this operation.
    case ERROR_WRITE_PROTECT:  // The media is write protected.
      return ERR_ACCESS_DENIED;
    case ERROR_SHARING_VIOLATION:  // Cannot access the file because it is
      return ERR_ACCESS_DENIED;    // being used by another process.
    case ERROR_LOCK_VIOLATION:   // The process cannot access the file because
      return ERR_ACCESS_DENIED;  // another process has locked the file.
    case ERROR_HANDLE_EOF:  // Reached the end of the file.
      return ERR_FAILED;
    case ERROR_HANDLE_DISK_FULL:  // The disk is full.
      return ERR_FILE_NO_SPACE;
    case ERROR_FILE_EXISTS:  // The file exists.
      return ERR_FILE_EXISTS;
    case ERROR_INVALID_PARAMETER:  // The parameter is incorrect.
      return ERR_INVALID_ARGUMENT;
    case ERROR_BUFFER_OVERFLOW:  // The file name is too long.
      return ERR_FILE_PATH_TOO_LONG;
    case ERROR_DISK_FULL:  // There is not enough space on the disk.
      return ERR_FILE_NO_SPACE;
    case ERROR_CALL_NOT_IMPLEMENTED:  // This function is not supported on
      return ERR_NOT_IMPLEMENTED;     // this system.
    case ERROR_INVALID_NAME:        // The filename, directory name, or volume
      return ERR_INVALID_ARGUMENT;  // label syntax is incorrect.
    case ERROR_DIR_NOT_EMPTY:  // The directory is not empty.
      return ERR_FAILED;
    case ERROR_BUSY:  // The requested resource is in use.
      return ERR_ACCESS_DENIED;
    case ERROR_ALREADY_EXISTS:  // Cannot create a file when that file
      return ERR_FILE_EXISTS;   // already exists.
    case ERROR_FILENAME_EXCED_RANGE:  // The filename or extension is too long.
      return ERR_FILE_PATH_TOO_LONG;
    case ERROR_FILE_TOO_LARGE:   // The file size exceeds the limit allowed
      return ERR_FILE_NO_SPACE;  // and cannot be saved.
    case ERROR_VIRUS_INFECTED:         // Operation failed because the file
      return ERR_FILE_VIRUS_INFECTED;  // contains a virus.
    case ERROR_IO_DEVICE:        // The request could not be performed
      return ERR_ACCESS_DENIED;  // because of an I/O device error.
    case ERROR_POSSIBLE_DEADLOCK:  // A potential deadlock condition has
      return ERR_ACCESS_DENIED;    // been detected.
    case ERROR_BAD_DEVICE:  // The specified device name is invalid.
      return ERR_INVALID_ARGUMENT;
    case ERROR_BROKEN_PIPE:  // Pipe is not connected.
      return ERR_CONNECTION_RESET;

    case ERROR_SUCCESS:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << os_error
                   << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

}  // namespace net
