// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_MOJO_MOJO_WATCH_CALLBACK_H_
#define SERVICES_ACCESSIBILITY_FEATURES_MOJO_MOJO_WATCH_CALLBACK_H_

#include <memory>

#include "base/sequence_checker.h"
#include "gin/public/context_holder.h"
#include "mojo/public/c/system/types.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-persistent-handle.h"

namespace ax {

// A class wrapping a v8 function which can execute that function
// to send a result to Javascript.
// Similar to blink::V8MojoWatchCallback, defined in
// third_party/blink/renderer/core/mojo/mojo_handle.idl.
class MojoWatchCallback {
 public:
  MojoWatchCallback(std::unique_ptr<gin::ContextHolder> context_holder,
                    v8::Local<v8::Function> callback);
  ~MojoWatchCallback();
  MojoWatchCallback(const MojoWatchCallback&) = delete;
  MojoWatchCallback& operator=(const MojoWatchCallback&) = delete;

  // Sends the MojoResult to Javascript by calling the bound callback.
  void Call(MojoResult result);

 private:
  std::unique_ptr<gin::ContextHolder> context_holder_;

  // Keep a reference to the callback which ensures the callback will not be
  // deleted until `this` goes out of scope.
  v8::Persistent<v8::Function> callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_MOJO_MOJO_WATCH_CALLBACK_H_
