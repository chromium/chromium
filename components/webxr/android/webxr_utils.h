// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_WEBXR_UTILS_H_
#define COMPONENTS_WEBXR_ANDROID_WEBXR_UTILS_H_

#include "base/android/jni_android.h"

namespace content {
struct GlobalRenderFrameHostId;
class WebContents;
}  // namespace content

// Functions in this file are currently GVR/ArCore specific functions. If other
// platforms need the same function here, please move it to
// components/webxr/*util.cc|h
namespace webxr {

content::WebContents* GetWebContents(
    const content::GlobalRenderFrameHostId& frame_id);

base::android::ScopedJavaLocalRef<jobject> GetJavaWebContents(
    const content::GlobalRenderFrameHostId& frame_id);

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_WEBXR_UTILS_H_
