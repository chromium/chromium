// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/jni_hdr_metadata.h"

#include "ui/gfx/hdr_metadata.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/base/android/media_jni_headers/HdrMetadata_jni.h"

namespace media {

namespace {

constexpr skhdr::ContentLightLevelInformation kDefaultCLLI;
constexpr skhdr::MasteringDisplayColorVolume kDefaultMDCV;

}  // namespace

JniHdrMetadata::JniHdrMetadata(const gfx::HDRMetadata& hdr_metadata)
    : hdr_metadata_(hdr_metadata) {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject_ = Java_HdrMetadata_create(env, reinterpret_cast<int64_t>(this));
  base::android::CheckException(env);
}

JniHdrMetadata::~JniHdrMetadata() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_HdrMetadata_close(env, obj());
}

float JniHdrMetadata::PrimaryRChromaticityX(JNIEnv* env) {
  return (hdr_metadata_->HasMDCV() ? hdr_metadata_->GetMDCV() : kDefaultMDCV)
      .fDisplayPrimaries.fRX;
}

float JniHdrMetadata::PrimaryRChromaticityY(JNIEnv* env) {
  return (hdr_metadata_->HasMDCV() ? hdr_metadata_->GetMDCV() : kDefaultMDCV)
      .fDisplayPrimaries.fRY;
}

float JniHdrMetadata::PrimaryGChromaticityX(JNIEnv* env) {
  return (hdr_metadata_->HasMDCV() ? hdr_metadata_->GetMDCV() : kDefaultMDCV)
      .fDisplayPrimaries.fGX;
}

float JniHdrMetadata::PrimaryGChromaticityY(JNIEnv* env) {
  return (hdr_metadata_->HasMDCV() ? hdr_metadata_->GetMDCV() : kDefaultMDCV)
      .fDisplayPrimaries.fGY;
}

float JniHdrMetadata::PrimaryBChromaticityX(JNIEnv* env) {
  return (hdr_metadata_->HasMDCV() ? hdr_metadata_->GetMDCV() : kDefaultMDCV)
      .fDisplayPrimaries.fBX;
}

float JniHdrMetadata::PrimaryBChromaticityY(JNIEnv* env) {
  return (hdr_metadata_->HasMDCV() ? hdr_metadata_->GetMDCV() : kDefaultMDCV)
      .fDisplayPrimaries.fBY;
}

float JniHdrMetadata::WhitePointChromaticityX(JNIEnv* env) {
  return (hdr_metadata_->HasMDCV() ? hdr_metadata_->GetMDCV() : kDefaultMDCV)
      .fDisplayPrimaries.fWX;
}

float JniHdrMetadata::WhitePointChromaticityY(JNIEnv* env) {
  return (hdr_metadata_->HasMDCV() ? hdr_metadata_->GetMDCV() : kDefaultMDCV)
      .fDisplayPrimaries.fWY;
}

float JniHdrMetadata::MaxColorVolumeLuminance(JNIEnv* env) {
  return (hdr_metadata_->HasMDCV() ? hdr_metadata_->GetMDCV() : kDefaultMDCV)
      .fMaximumDisplayMasteringLuminance;
}

float JniHdrMetadata::MinColorVolumeLuminance(JNIEnv* env) {
  return (hdr_metadata_->HasMDCV() ? hdr_metadata_->GetMDCV() : kDefaultMDCV)
      .fMinimumDisplayMasteringLuminance;
}

int32_t JniHdrMetadata::MaxContentLuminance(JNIEnv* env) {
  if (hdr_metadata_->HasCLLI()) {
    return hdr_metadata_->GetCLLI().getUint16MaxCLL();
  }
  return kDefaultCLLI.getUint16MaxCLL();
}

int32_t JniHdrMetadata::MaxFrameAverageLuminance(JNIEnv* env) {
  if (hdr_metadata_->HasCLLI()) {
    return hdr_metadata_->GetCLLI().getUint16MaxFALL();
  }
  return kDefaultCLLI.getUint16MaxFALL();
}

}  // namespace media

DEFINE_JNI(HdrMetadata)
