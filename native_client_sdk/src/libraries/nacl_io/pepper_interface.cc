// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/pepper_interface.h"
#include <errno.h>
#include <ppapi/c/pp_errors.h>

#include "nacl_io/log.h"

namespace nacl_io {

void PepperInterface::AddRefResource(PP_Resource resource) {
  GetCoreInterface()->AddRefResource(resource);
}

void PepperInterface::ReleaseResource(PP_Resource resource) {
  GetCoreInterface()->ReleaseResource(resource);
}

ScopedResource::ScopedResource(PepperInterface* ppapi)
    : ppapi_(ppapi), resource_(0) {
}

ScopedResource::ScopedResource(PepperInterface* ppapi, PP_Resource resource)
    : ppapi_(ppapi), resource_(resource) {
}

ScopedResource::~ScopedResource() {
  if (resource_)
    ppapi_->ReleaseResource(resource_);
}

void ScopedResource::Reset(PP_Resource resource) {
  if (resource_)
    ppapi_->ReleaseResource(resource_);

  resource_ = resource;
}

PP_Resource ScopedResource::Release() {
  PP_Resource result = resource_;
  resource_ = 0;
  return result;
}

ScopedVar::ScopedVar(PepperInterface* ppapi)
    : ppapi_(ppapi), var_(PP_MakeUndefined()) {}

ScopedVar::ScopedVar(PepperInterface* ppapi, PP_Var var)
    : ppapi_(ppapi), var_(var) {}

ScopedVar::~ScopedVar() {
  ppapi_->GetVarInterface()->Release(var_);
}

void ScopedVar::Reset(PP_Var var) {
  ppapi_->GetVarInterface()->Release(var_);
  var_ = var;
}

PP_Var ScopedVar::Release() {
  PP_Var result = var_;
  var_ = PP_MakeUndefined();
  return result;
}

int PPErrorToErrno(int32_t err) {
  // If not an error, then just return it.
  if (err >= PP_OK)
    return err;

  switch (err) {
    case PP_OK_COMPLETIONPENDING: return 0;
    case PP_ERROR_FAILED: return EPERM;
    case PP_ERROR_ABORTED: return EPERM;
    case PP_ERROR_BADARGUMENT: return EINVAL;
    case PP_ERROR_BADRESOURCE: return EBADF;
    case PP_ERROR_NOINTERFACE: return ENOSYS;
    case PP_ERROR_NOACCESS: return EACCES;
    case PP_ERROR_NOMEMORY: return ENOMEM;
    case PP_ERROR_NOSPACE: return ENOSPC;
    case PP_ERROR_NOQUOTA: return ENOSPC;
    case PP_ERROR_INPROGRESS: return EBUSY;
    case PP_ERROR_NOTSUPPORTED: return ENOSYS;
    case PP_ERROR_BLOCKS_MAIN_THREAD: return EPERM;
    case PP_ERROR_FILENOTFOUND: return ENOENT;
    case PP_ERROR_FILEEXISTS: return EEXIST;
    case PP_ERROR_FILETOOBIG: return EFBIG;
    case PP_ERROR_FILECHANGED: return EINVAL;
    case PP_ERROR_TIMEDOUT: return EBUSY;
    case PP_ERROR_USERCANCEL: return EPERM;
    case PP_ERROR_NO_USER_GESTURE: return EPERM;
    case PP_ERROR_CONTEXT_LOST: return EPERM;
    case PP_ERROR_NO_MESSAGE_LOOP: return EPERM;
    case PP_ERROR_WRONG_THREAD: return EPERM;
    case PP_ERROR_CONNECTION_ABORTED: return ECONNABORTED;
    case PP_ERROR_CONNECTION_REFUSED: return ECONNREFUSED;
    case PP_ERROR_CONNECTION_FAILED: return ECONNREFUSED;
    case PP_ERROR_CONNECTION_TIMEDOUT: return ETIMEDOUT;
    case PP_ERROR_ADDRESS_UNREACHABLE: return ENETUNREACH;
    case PP_ERROR_ADDRESS_IN_USE: return EADDRINUSE;
  }

  return EINVAL;
}

#if !defined(NDEBUG)

int PPErrorToErrnoLog(int32_t err, const char* file, int line) {
  if (err >= PP_OK)
    return err;

#define PP_ERRORS(V)              \
  V(PP_OK)                        \
  V(PP_OK_COMPLETIONPENDING)      \
  V(PP_ERROR_FAILED)              \
  V(PP_ERROR_ABORTED)             \
  V(PP_ERROR_BADARGUMENT)         \
  V(PP_ERROR_BADRESOURCE)         \
  V(PP_ERROR_NOINTERFACE)         \
  V(PP_ERROR_NOACCESS)            \
  V(PP_ERROR_NOMEMORY)            \
  V(PP_ERROR_NOSPACE)             \
  V(PP_ERROR_NOQUOTA)             \
  V(PP_ERROR_INPROGRESS)          \
  V(PP_ERROR_NOTSUPPORTED)        \
  V(PP_ERROR_BLOCKS_MAIN_THREAD)  \
  V(PP_ERROR_MALFORMED_INPUT)     \
  V(PP_ERROR_RESOURCE_FAILED)     \
  V(PP_ERROR_FILENOTFOUND)        \
  V(PP_ERROR_FILEEXISTS)          \
  V(PP_ERROR_FILETOOBIG)          \
  V(PP_ERROR_FILECHANGED)         \
  V(PP_ERROR_NOTAFILE)            \
  V(PP_ERROR_TIMEDOUT)            \
  V(PP_ERROR_USERCANCEL)          \
  V(PP_ERROR_NO_USER_GESTURE)     \
  V(PP_ERROR_CONTEXT_LOST)        \
  V(PP_ERROR_NO_MESSAGE_LOOP)     \
  V(PP_ERROR_WRONG_THREAD)        \
  V(PP_ERROR_WOULD_BLOCK_THREAD)  \
  V(PP_ERROR_CONNECTION_CLOSED)   \
  V(PP_ERROR_CONNECTION_RESET)    \
  V(PP_ERROR_CONNECTION_REFUSED)  \
  V(PP_ERROR_CONNECTION_ABORTED)  \
  V(PP_ERROR_CONNECTION_FAILED)   \
  V(PP_ERROR_CONNECTION_TIMEDOUT) \
  V(PP_ERROR_ADDRESS_INVALID)     \
  V(PP_ERROR_ADDRESS_UNREACHABLE) \
  V(PP_ERROR_ADDRESS_IN_USE)      \
  V(PP_ERROR_MESSAGE_TOO_BIG)     \
  V(PP_ERROR_NAME_NOT_RESOLVED)

#define ERROR_STRING_PAIR(x) {x, #x},

  const struct {
    int err;
    const char* name;
  } kErrorStringPair[] = {
    PP_ERRORS(ERROR_STRING_PAIR)
  };

#undef ERROR_STRING_PAIR
#undef PP_ERRORS

  const char* err_string = "Unknown PPError value";
  for (size_t i = 0; i < sizeof(kErrorStringPair) / sizeof(kErrorStringPair[0]);
       ++i) {
    if (err == kErrorStringPair[i].err) {
      err_string = kErrorStringPair[i].name;
    }
  }

  nacl_io_log(LOG_PREFIX "%s:%d: Got PPError %d = %s\n", file, line, err,
              err_string);

  return PPErrorToErrno(err);
}

#endif

}  // namespace nacl_io
