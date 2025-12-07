// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_JNI_HDR_METADATA_H_
#define MEDIA_BASE_ANDROID_JNI_HDR_METADATA_H_

#include "base/android/jni_android.h"
#include "base/memory/raw_ref.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

class VideoColorSpace;

class JniHdrMetadata {
 public:
  JniHdrMetadata(const VideoColorSpace& color_space,
                 const gfx::HDRMetadata& hdr_metadata);

  JniHdrMetadata(const JniHdrMetadata&) = delete;
  JniHdrMetadata& operator=(const JniHdrMetadata&) = delete;

  ~JniHdrMetadata();

  base::android::ScopedJavaLocalRef<jobject> obj() { return jobject_; }

  // Java HdrMetadata implementation.

  jint Primaries(JNIEnv* env);
  jint ColorTransfer(JNIEnv* env);
  jint Range(JNIEnv* env);

  jfloat PrimaryRChromaticityX(JNIEnv* env);
  jfloat PrimaryRChromaticityY(JNIEnv* env);
  jfloat PrimaryGChromaticityX(JNIEnv* env);
  jfloat PrimaryGChromaticityY(JNIEnv* env);
  jfloat PrimaryBChromaticityX(JNIEnv* env);
  jfloat PrimaryBChromaticityY(JNIEnv* env);
  jfloat WhitePointChromaticityX(JNIEnv* env);
  jfloat WhitePointChromaticityY(JNIEnv* env);
  jfloat MaxColorVolumeLuminance(JNIEnv* env);
  jfloat MinColorVolumeLuminance(JNIEnv* env);
  jint MaxContentLuminance(JNIEnv* env);
  jint MaxFrameAverageLuminance(JNIEnv* env);

 private:
  const raw_ref<const VideoColorSpace> color_space_;
  const raw_ref<const gfx::HDRMetadata> hdr_metadata_;
  base::android::ScopedJavaLocalRef<jobject> jobject_;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_JNI_HDR_METADATA_H_
