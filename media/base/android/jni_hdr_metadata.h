// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_JNI_HDR_METADATA_H_
#define MEDIA_BASE_ANDROID_JNI_HDR_METADATA_H_

#include "base/android/jni_android.h"
#include "base/memory/raw_ref.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

class JniHdrMetadata {
 public:
  explicit JniHdrMetadata(const gfx::HDRMetadata& hdr_metadata);

  JniHdrMetadata(const JniHdrMetadata&) = delete;
  JniHdrMetadata& operator=(const JniHdrMetadata&) = delete;

  ~JniHdrMetadata();

  base::android::ScopedJavaLocalRef<jobject> obj() { return jobject_; }

  // Java HdrMetadata implementation.

  float PrimaryRChromaticityX(JNIEnv* env);
  float PrimaryRChromaticityY(JNIEnv* env);
  float PrimaryGChromaticityX(JNIEnv* env);
  float PrimaryGChromaticityY(JNIEnv* env);
  float PrimaryBChromaticityX(JNIEnv* env);
  float PrimaryBChromaticityY(JNIEnv* env);
  float WhitePointChromaticityX(JNIEnv* env);
  float WhitePointChromaticityY(JNIEnv* env);
  float MaxColorVolumeLuminance(JNIEnv* env);
  float MinColorVolumeLuminance(JNIEnv* env);
  int32_t MaxContentLuminance(JNIEnv* env);
  int32_t MaxFrameAverageLuminance(JNIEnv* env);

 private:
  const raw_ref<const gfx::HDRMetadata> hdr_metadata_;
  base::android::ScopedJavaLocalRef<jobject> jobject_;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_JNI_HDR_METADATA_H_
