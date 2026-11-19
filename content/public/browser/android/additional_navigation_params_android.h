// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_ADDITIONAL_NAVIGATION_PARAMS_ANDROID_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_ADDITIONAL_NAVIGATION_PARAMS_ANDROID_H_

#include <optional>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"
#include "content/public/common/child_process_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

class InitiatorNavigationState;

// Creates a Java AdditionalNavigationParams carrying explicit initiator frame
// token, process ID, and navigation state. Because the params may be consumed
// after the initiator frame has been destroyed, this holds a reference inside
// the Java object that keeps the frame's navigation-relevant state available;
// callers must ensure the returned Java object is destroyed when no longer
// needed.
CONTENT_EXPORT base::android::ScopedJavaLocalRef<jobject>
CreateJavaAdditionalNavigationParams(
    JNIEnv* env,
    const std::optional<blink::LocalFrameToken>& initiator_frame_token,
    ChildProcessId initiator_process_id,
    scoped_refptr<InitiatorNavigationState> initiator_navigation_state);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_ADDITIONAL_NAVIGATION_PARAMS_ANDROID_H_
