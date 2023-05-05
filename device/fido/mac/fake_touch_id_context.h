// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_FAKE_TOUCH_ID_CONTEXT_H_
#define DEVICE_FIDO_MAC_FAKE_TOUCH_ID_CONTEXT_H_

#include "base/component_export.h"
#include "device/fido/mac/touch_id_context.h"

namespace device::fido::mac {

class FakeTouchIdContext : public TouchIdContext {
 public:
  FakeTouchIdContext(const FakeTouchIdContext&) = delete;
  FakeTouchIdContext& operator=(const FakeTouchIdContext&) = delete;

  ~FakeTouchIdContext() override;

  // Will prevent the next call to PromptTouchId from running the callback.
  void DoNotResolveNextPrompt();

  // TouchIdContext:
  void PromptTouchId(const std::u16string& reason, Callback callback) override;

  void set_callback_result(bool callback_result) {
    callback_result_ = callback_result;
  }

 private:
  friend class ScopedTouchIdTestEnvironment;

  FakeTouchIdContext();

  bool callback_result_ = true;
  bool resolve_next_prompt_ = true;
};

}  // namespace device::fido::mac

#endif  // DEVICE_FIDO_MAC_FAKE_TOUCH_ID_CONTEXT_H_
