// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/test/android/content_test_jni/FencedFrameUtils_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

jint JNI_FencedFrameUtils_GetCount(JNIEnv* env,
                                   const JavaParamRef<jobject>& jrfh) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      content::RenderFrameHost::FromJavaRenderFrameHost(jrfh));
  DCHECK(rfh);
  return rfh->GetFencedFrames().size();
}

ScopedJavaLocalRef<jobject> JNI_FencedFrameUtils_GetLastFencedFrame(
    JNIEnv* env,
    const JavaParamRef<jobject>& jrfh,
    const JavaParamRef<jstring>& jurl) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      content::RenderFrameHost::FromJavaRenderFrameHost(jrfh));
  DCHECK(rfh);

  std::vector<FencedFrame*> fenced_frames = rfh->GetFencedFrames();
  FencedFrame* fenced_frame = fenced_frames.back();
  DCHECK(fenced_frame);

  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));

  // Return the main RenderFrameHost of the most-recently added fenced frame if
  // the navigation committed successfully.
  if (fenced_frame->GetInnerRoot()->GetLastCommittedURL() == url)
    return fenced_frame->GetInnerRoot()->GetJavaRenderFrameHost();
  return ScopedJavaLocalRef<jobject>();
}

}  // namespace content
