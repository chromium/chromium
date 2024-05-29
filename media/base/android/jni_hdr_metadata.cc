// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/jni_hdr_metadata.h"

#include "media/base/video_color_space.h"
#include "ui/gfx/hdr_metadata.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/base/android/media_jni_headers/HdrMetadata_jni.h"

namespace media {

namespace {

constexpr gfx::HdrMetadataCta861_3 kDefault861_3;
constexpr gfx::HdrMetadataSmpteSt2086 kDefault2086;

}  // namespace

JniHdrMetadata::JniHdrMetadata(const VideoColorSpace& color_space,
                               const gfx::HDRMetadata& hdr_metadata)
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
  return static_cast<int>(color_space_->primaries);
}

jint JniHdrMetadata::ColorTransfer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return static_cast<int>(color_space_->transfer);
}

jint JniHdrMetadata::Range(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj) {
  return static_cast<int>(color_space_->range);
}

jfloat JniHdrMetadata::PrimaryRChromaticityX(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_->smpte_st_2086.value_or(kDefault2086).primaries.fRX;
}

jfloat JniHdrMetadata::PrimaryRChromaticityY(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_->smpte_st_2086.value_or(kDefault2086).primaries.fRY;
}

jfloat JniHdrMetadata::PrimaryGChromaticityX(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_->smpte_st_2086.value_or(kDefault2086).primaries.fGX;
}

jfloat JniHdrMetadata::PrimaryGChromaticityY(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_->smpte_st_2086.value_or(kDefault2086).primaries.fGY;
}

jfloat JniHdrMetadata::PrimaryBChromaticityX(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_->smpte_st_2086.value_or(kDefault2086).primaries.fBX;
}

jfloat JniHdrMetadata::PrimaryBChromaticityY(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_->smpte_st_2086.value_or(kDefault2086).primaries.fBY;
}

jfloat JniHdrMetadata::WhitePointChromaticityX(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_->smpte_st_2086.value_or(kDefault2086).primaries.fWX;
}

jfloat JniHdrMetadata::WhitePointChromaticityY(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_->smpte_st_2086.value_or(kDefault2086).primaries.fWY;
}

jfloat JniHdrMetadata::MaxColorVolumeLuminance(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_->smpte_st_2086.value_or(kDefault2086).luminance_max;
}

jfloat JniHdrMetadata::MinColorVolumeLuminance(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_->smpte_st_2086.value_or(kDefault2086).luminance_min;
}

jint JniHdrMetadata::MaxContentLuminance(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_->cta_861_3.value_or(kDefault861_3)
      .max_content_light_level;
}

jint JniHdrMetadata::MaxFrameAverageLuminance(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return hdr_metadata_->cta_861_3.value_or(kDefault861_3)
      .max_frame_average_light_level;
}

}  // namespace media
