// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/host/error_conversion.h"

#include "base/numerics/safe_conversions.h"
#include "net/base/net_errors.h"
#include "ppapi/c/pp_errors.h"

namespace ppapi {
namespace host {

int32_t NetErrorToPepperError(int net_error) {
  if (net_error > 0)
    return base::checked_cast<int32_t>(net_error);

  switch (net_error) {
    case net::OK:
      return PP_OK;
    case net::ERR_IO_PENDING:
      return PP_OK_COMPLETIONPENDING;
    case net::ERR_ABORTED:
      return PP_ERROR_ABORTED;
    case net::ERR_INVALID_ARGUMENT:
      return PP_ERROR_BADARGUMENT;
    case net::ERR_INVALID_HANDLE:
      return PP_ERROR_BADARGUMENT;
    case net::ERR_FILE_NOT_FOUND:
      return PP_ERROR_FILENOTFOUND;
    case net::ERR_TIMED_OUT:
      return PP_ERROR_TIMEDOUT;
    case net::ERR_FILE_TOO_BIG:
      return PP_ERROR_FILETOOBIG;
    case net::ERR_ACCESS_DENIED:
      return PP_ERROR_NOACCESS;
    case net::ERR_NOT_IMPLEMENTED:
      return PP_ERROR_NOTSUPPORTED;
    case net::ERR_OUT_OF_MEMORY:
      return PP_ERROR_NOMEMORY;
    case net::ERR_FILE_EXISTS:
      return PP_ERROR_FILEEXISTS;
    case net::ERR_FILE_NO_SPACE:
      return PP_ERROR_NOSPACE;
    case net::ERR_CONNECTION_CLOSED:
      return PP_ERROR_CONNECTION_CLOSED;
    case net::ERR_CONNECTION_RESET:
      return PP_ERROR_CONNECTION_RESET;
    case net::ERR_CONNECTION_REFUSED:
      return PP_ERROR_CONNECTION_REFUSED;
    case net::ERR_CONNECTION_ABORTED:
      return PP_ERROR_CONNECTION_ABORTED;
    case net::ERR_CONNECTION_FAILED:
      return PP_ERROR_CONNECTION_FAILED;
    case net::ERR_NAME_NOT_RESOLVED:
    case net::ERR_ICANN_NAME_COLLISION:
      return PP_ERROR_NAME_NOT_RESOLVED;
    case net::ERR_ADDRESS_INVALID:
      return PP_ERROR_ADDRESS_INVALID;
    case net::ERR_ADDRESS_UNREACHABLE:
      return PP_ERROR_ADDRESS_UNREACHABLE;
    case net::ERR_CONNECTION_TIMED_OUT:
      return PP_ERROR_CONNECTION_TIMEDOUT;
    case net::ERR_NETWORK_ACCESS_DENIED:
      return PP_ERROR_NOACCESS;
    case net::ERR_MSG_TOO_BIG:
      return PP_ERROR_MESSAGE_TOO_BIG;
    case net::ERR_ADDRESS_IN_USE:
      return PP_ERROR_ADDRESS_IN_USE;
    default:
      return PP_ERROR_FAILED;
  }
}

}  // namespace host
}  // namespace ppapi
