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

  jint Primaries(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  jint ColorTransfer(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  jint Range(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  jfloat PrimaryRChromaticityX(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  jfloat PrimaryRChromaticityY(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  jfloat PrimaryGChromaticityX(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  jfloat PrimaryGChromaticityY(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  jfloat PrimaryBChromaticityX(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  jfloat PrimaryBChromaticityY(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  jfloat WhitePointChromaticityX(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jfloat WhitePointChromaticityY(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jfloat MaxColorVolumeLuminance(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jfloat MinColorVolumeLuminance(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jint MaxContentLuminance(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);
  jint MaxFrameAverageLuminance(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 private:
  const raw_ref<const VideoColorSpace> color_space_;
  const raw_ref<const gfx::HDRMetadata> hdr_metadata_;
  base::android::ScopedJavaLocalRef<jobject> jobject_;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_JNI_HDR_METADATA_H_
