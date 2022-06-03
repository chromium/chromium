// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/fake_touch_id_context.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"

namespace device {
namespace fido {
namespace mac {

FakeTouchIdContext::FakeTouchIdContext() = default;
FakeTouchIdContext::~FakeTouchIdContext() = default;

void FakeTouchIdContext::PromptTouchId(const std::u16string& reason,
                                       Callback callback) {
  std::move(callback).Run(callback_result_);
}

}  // namespace mac
}  // namespace fido
}  // namespace device
