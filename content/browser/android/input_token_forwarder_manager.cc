// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/input_token_forwarder_manager.h"

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/InputTokenForwarderManager_jni.h"

namespace content {

// static
InputTokenForwarderManager* InputTokenForwarderManager::GetInstance() {
  return base::Singleton<
      InputTokenForwarderManager,
      base::LeakySingletonTraits<InputTokenForwarderManager>>::get();
}

void InputTokenForwarderManager::ForwardVizInputTransferToken(
    int surface_id,
    const jni_zero::JavaRef<>& viz_input_token) {
  Java_InputTokenForwarderManager_onTokenReceived(
      base::android::AttachCurrentThread(), surface_id, viz_input_token);
}

}  // namespace content

DEFINE_JNI(InputTokenForwarderManager)
