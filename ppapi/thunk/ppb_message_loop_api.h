// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_MESSAGE_LOOP_API_H_
#define PPAPI_THUNK_PPB_MESSAGE_LOOP_API_H_

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_MessageLoop_API {
 public:
  virtual ~PPB_MessageLoop_API() {}

  virtual int32_t AttachToCurrentThread() = 0;
  virtual int32_t Run() = 0;
  // Note: Most interfaces should use scoped_refptr<TrackedCallback>, in order
  // to track callbacks and support things like blocking or optional callbacks.
  // In this case, the callback is really just a way to pass a function pointer,
  // and those options don't make sense.
  virtual int32_t PostWork(PP_CompletionCallback callback,
                           int64_t delay_ms) = 0;
  virtual int32_t PostQuit(PP_Bool should_destroy) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_MESSAGE_LOOP_API_H_
