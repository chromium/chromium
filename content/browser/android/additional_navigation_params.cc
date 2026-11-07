// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/additional_navigation_params.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/android/unguessable_token_android.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/android/child_process_id.h"
#include "content/public/browser/initiator_navigation_state.h"
#include "third_party/jni_zero/jni_zero.h"

namespace content {

struct InitiatorNavigationStateWrapper {
  explicit InitiatorNavigationStateWrapper(
      scoped_refptr<InitiatorNavigationState> state)
      : state(std::move(state)) {}
  ~InitiatorNavigationStateWrapper() = default;

  scoped_refptr<InitiatorNavigationState> state;
};

}  // namespace content

#include "content/public/android/content_jni_headers/AdditionalNavigationParamsImpl_jni.h"

namespace content {

base::android::ScopedJavaLocalRef<jobject> CreateJavaAdditionalNavigationParams(
    JNIEnv* env,
    RenderFrameHost& initiator_frame_host) {
  auto* rfhi = static_cast<RenderFrameHostImpl*>(&initiator_frame_host);
  scoped_refptr<InitiatorNavigationState> initiator_navigation_state =
      rfhi->GetCurrentInitiatorNavigationState();
  jni_zero::JniUniquePtr<InitiatorNavigationStateWrapper> wrapper;
  if (initiator_navigation_state) {
    wrapper = jni_zero::MakeUnique<InitiatorNavigationStateWrapper>(
        std::move(initiator_navigation_state));
  }
  return Java_AdditionalNavigationParamsImpl_Constructor(
      env, initiator_frame_host.GetFrameToken().value(),
      initiator_frame_host.GetProcess()->GetID(), std::move(wrapper));
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
  InitiatorNavigationStateWrapper* wrapper =
      Java_AdditionalNavigationParamsImpl_getNativeWrapper(env, j_object);
  scoped_refptr<InitiatorNavigationState> state =
      wrapper ? std::move(wrapper->state) : nullptr;
  Java_AdditionalNavigationParamsImpl_destroy(env, j_object);
  return state;
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
