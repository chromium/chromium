// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/jni_hdr_metadata.h"

#include "media/base/android/media_jni_headers/HdrMetadata_jni.h"
#include "media/base/hdr_metadata.h"
#include "media/base/video_color_space.h"

namespace media {

JniHdrMetadata::JniHdrMetadata(const VideoColorSpace& color_space,
                               const HDRMetadata& hdr_metadata)
    : color_space_(color_space), hdr_metadata_(hdr_metadata) {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject_ = Java_HdrMetadata_create(env, reinterpret_cast<jlong>(this));
  base::android::CheckException(env);
}

JniHdrMetadata::~JniHdrMetadata() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_HdrMetadata_close(env, obj());
}

jint JniHdrMetadata::Primaries(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return static_cast<int>(color_space_.primaries);
}

jint JniHdrMetadata::ColorTransfer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return static_cast<int>(color_space_.transfer);
}

jint JniHdrMetadata::Range(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj) {
  return static_cast<int>(color_space_.range);
}

jfloat JniHdrMetadata::PrimaryRChromaticityX(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_.mastering_metadata.primary_r.x();
}

jfloat JniHdrMetadata::PrimaryRChromaticityY(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_.mastering_metadata.primary_r.y();
}

jfloat JniHdrMetadata::PrimaryGChromaticityX(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_.mastering_metadata.primary_g.x();
}

jfloat JniHdrMetadata::PrimaryGChromaticityY(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_.mastering_metadata.primary_g.y();
}

jfloat JniHdrMetadata::PrimaryBChromaticityX(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_.mastering_metadata.primary_b.x();
}

jfloat JniHdrMetadata::PrimaryBChromaticityY(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_.mastering_metadata.primary_b.y();
}

jfloat JniHdrMetadata::WhitePointChromaticityX(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_.mastering_metadata.white_point.x();
}

jfloat JniHdrMetadata::WhitePointChromaticityY(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_.mastering_metadata.white_point.y();
}

jfloat JniHdrMetadata::MaxMasteringLuminance(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_.mastering_metadata.luminance_max;
}

jfloat JniHdrMetadata::MinMasteringLuminance(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_.mastering_metadata.luminance_min;
}

jint JniHdrMetadata::MaxContentLuminance(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_.max_content_light_level;
}

jint JniHdrMetadata::MaxFrameAverageLuminance(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_.max_frame_average_light_level;
}

}  // namespace media
