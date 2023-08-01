// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/fake_touch_id_context.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"

namespace device::fido::mac {

FakeTouchIdContext::FakeTouchIdContext() = default;
FakeTouchIdContext::~FakeTouchIdContext() = default;

void FakeTouchIdContext::DoNotResolveNextPrompt() {
  resolve_next_prompt_ = false;
}

void FakeTouchIdContext::PromptTouchId(const std::u16string& reason,
                                       Callback callback) {
  if (resolve_next_prompt_) {
    std::move(callback).Run(callback_result_);
  }
  // After running the callback it is expected that the object will be
  // destroyed, so no members should be used beyond this point.
}

}  // namespace device::fido::mac
