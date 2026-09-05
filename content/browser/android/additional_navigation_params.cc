// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/additional_navigation_params.h"

#include "base/android/jni_string.h"
#include "base/android/unguessable_token_android.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/android/child_process_id.h"
#include "content/public/android/content_jni_headers/AdditionalNavigationParamsImpl_jni.h"
#include "content/public/browser/initiator_navigation_state.h"

namespace content {

base::android::ScopedJavaLocalRef<jobject> CreateJavaAdditionalNavigationParams(
    JNIEnv* env,
    RenderFrameHost& initiator_frame_host) {
  auto* rfhi = static_cast<RenderFrameHostImpl*>(&initiator_frame_host);
  scoped_refptr<InitiatorNavigationState> initiator_navigation_state =
      rfhi->GetCurrentInitiatorNavigationState();
  int64_t native_state_ptr = 0;
  if (initiator_navigation_state) {
    initiator_navigation_state->AddRef();
    native_state_ptr =
        reinterpret_cast<int64_t>(initiator_navigation_state.get());
  }
  return Java_AdditionalNavigationParamsImpl_Constructor(
      env, initiator_frame_host.GetFrameToken().value(),
      initiator_frame_host.GetProcess()->GetID(), native_state_ptr);
}

void JNI_AdditionalNavigationParamsImpl_Destroy(JNIEnv* env,
                                                int64_t native_state) {
  if (native_state) {
    auto* state = reinterpret_cast<InitiatorNavigationState*>(native_state);
    state->Release();
  }
}

std::optional<blink::LocalFrameToken>
GetInitiatorFrameTokenFromJavaAdditionalNavigationParams(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_object) {
  if (!j_object) {
    return std::nullopt;
  }
  std::optional<base::UnguessableToken> optional_token =
      Java_AdditionalNavigationParamsImpl_getInitiatorFrameToken(env, j_object);
  if (optional_token) {
    return blink::LocalFrameToken(optional_token.value());
  }
  return std::nullopt;
}

content::ChildProcessId GetInitiatorProcessIdFromJavaAdditionalNavigationParams(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_object) {
  if (!j_object) {
    return content::ChildProcessId();
  }
  return Java_AdditionalNavigationParamsImpl_getInitiatorProcessId(env,
                                                                   j_object);
}

scoped_refptr<InitiatorNavigationState>
TakeNativeStateFromJavaAdditionalNavigationParams(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_object) {
  if (!j_object) {
    return nullptr;
  }
  int64_t native_ptr =
      Java_AdditionalNavigationParamsImpl_takeNativeState(env, j_object);
  if (!native_ptr) {
    return nullptr;
  }
  auto* state = reinterpret_cast<InitiatorNavigationState*>(native_ptr);
  auto ref_ptr = base::WrapRefCounted(state);
  state->Release();
  return ref_ptr;
}

void DestroyJavaAdditionalNavigationParams(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_object) {
  if (!j_object) {
    return;
  }
  Java_AdditionalNavigationParamsImpl_destroy(env, j_object);
}

}  // namespace content

DEFINE_JNI(AdditionalNavigationParamsImpl)
