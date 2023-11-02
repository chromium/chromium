// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_IMAGE_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_IMAGE_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)

#include <jni.h>

#include "base/android/scoped_java_ref.h"

#endif  // BUILDFLAG(IS_ANDROID)

namespace media_session {

// Structure representing an MediaImage as per the MediaSession API, see:
// https://wicg.github.io/mediasession/#dictdef-mediaimage
struct COMPONENT_EXPORT(MEDIA_SESSION_BASE_CPP) MediaImage {
  MediaImage();
  MediaImage(const MediaImage& other);
  ~MediaImage();

  bool operator==(const MediaImage& other) const;

#if BUILDFLAG(IS_ANDROID)
  // Creates a Java array of MediaImage instances and returns the JNI ref.
  static base::android::ScopedJavaLocalRef<jobjectArray> ToJavaArray(
      JNIEnv* env,
      const std::vector<MediaImage>& images);

  // Creates a Java MediaImage instance and returns the JNI ref.
  base::android::ScopedJavaLocalRef<jobject> CreateJavaObject(
      JNIEnv* env) const;
#endif

  // MUST be a valid url. If an icon doesn't have a valid URL, it will not be
  // successfully parsed, thus will not be represented in the Manifest.
  GURL src;

  // Empty if the parsing failed or the field was not present. The type can be
  // any string and doesn't have to be a valid image MIME type at this point.
  // It is up to the consumer of the object to check if the type matches a
  // supported type.
  std::u16string type;

  // Empty if the parsing failed, the field was not present or empty.
  // The special value "any" is represented by gfx::Size(0, 0).
  std::vector<gfx::Size> sizes;
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_IMAGE_H_
