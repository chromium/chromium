// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_ADDITIONAL_NAVIGATION_PARAMS_ANDROID_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_ADDITIONAL_NAVIGATION_PARAMS_ANDROID_H_

#include <optional>

#include "base/android/scoped_java_ref.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

// Creates a Java AdditionalNavigationParams describing `initiator_frame_host`
// as the initiator of a future navigation. Because the params may be consumed
// after `initiator_frame_host` has been destroyed, this holds a reference
// inside the Java object that keeps the frame's navigation-relevant state
// available; callers must ensure the Java object is destroyed when no longer
// needed.
CONTENT_EXPORT base::android::ScopedJavaLocalRef<jobject>
CreateJavaAdditionalNavigationParams(
    JNIEnv* env,
    RenderFrameHost& initiator_frame_host,
    std::optional<base::UnguessableToken> attribution_src_token);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_ADDITIONAL_NAVIGATION_PARAMS_ANDROID_H_
