// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_MESSAGE_LOOP_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_MESSAGE_LOOP_SHARED_H_

#include <stdint.h>

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_message_loop_api.h"

namespace base {
class Location;
class SingleThreadTaskRunner;
}

namespace ppapi {

// MessageLoopShared doesn't really do anything interesting. It exists so that
// shared code (in particular, TrackedCallback) can keep a pointer to a
// MessageLoop resource. In the host side, there is not a concrete class that
// implements this. So pointers to MessageLoopShared can only really be valid
// on the plugin side.
class PPAPI_SHARED_EXPORT MessageLoopShared
    : public Resource,
      public thunk::PPB_MessageLoop_API {
 public:
  explicit MessageLoopShared(PP_Instance instance);
  // Construct the one MessageLoopShared for the main thread. This must be
  // invoked on the main thread.
  struct ForMainThread {};
  explicit MessageLoopShared(ForMainThread);

  MessageLoopShared(const MessageLoopShared&) = delete;
  MessageLoopShared& operator=(const MessageLoopShared&) = delete;

  virtual ~MessageLoopShared();

  // Handles posting to the message loop if there is one, or the pending queue
  // if there isn't.
  // NOTE: The given closure will be run *WITHOUT* acquiring the Proxy lock.
  //       This only makes sense for user code and completely thread-safe
  //       proxy operations (e.g., MessageLoop::QuitClosure).
  virtual void PostClosure(const base::Location& from_here,
                           base::OnceClosure closure,
                           int64_t delay_ms) = 0;

  virtual base::SingleThreadTaskRunner* GetTaskRunner() = 0;

  // Returns whether this MessageLoop is currently handling a blocking message
  // from JavaScript. This is used to make it illegal to use blocking callbacks
  // while the thread is handling a blocking message.
  virtual bool CurrentlyHandlingBlockingMessage() = 0;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_MESSAGE_LOOP_SHARED_H_
