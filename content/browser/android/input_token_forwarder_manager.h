// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_INPUT_TOKEN_FORWARDER_MANAGER_H_
#define CONTENT_BROWSER_ANDROID_INPUT_TOKEN_FORWARDER_MANAGER_H_

#include "base/memory/singleton.h"
#include "components/input/android/input_token_forwarder.h"

namespace content {

// Class to be used for forwarding input tokens in case of `InProcessGpu`.
// The binder connection that is used to communicate between Viz and Browser,
// doesn't exist in case gpu is in process or when launched in single process
// mode.
class InputTokenForwarderManager : public input::InputTokenForwarder {
 public:
  static InputTokenForwarderManager* GetInstance();

  // InputTokenForwarder overrides.
  void ForwardVizInputTransferToken(
      int surface_id,
      base::android::ScopedJavaGlobalRef<jobject> viz_input_token) override;

 private:
  friend struct base::DefaultSingletonTraits<InputTokenForwarderManager>;

  ~InputTokenForwarderManager() override = default;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_INPUT_TOKEN_FORWARDER_MANAGER_H_
