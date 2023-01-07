// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/core.h"

#include "ppapi/cpp/completion_callback.h"

namespace pp {

// This function is implemented in the .cc file to avoid including completion
// callback all over the project.
void Core::CallOnMainThread(int32_t delay_in_milliseconds,
                            const CompletionCallback& callback,
                            int32_t result) {
  return interface_->CallOnMainThread(delay_in_milliseconds,
                                      callback.pp_completion_callback(),
                                      result);
}

bool Core::IsMainThread() {
  return PP_ToBool(interface_->IsMainThread());
}

}  // namespace pp
