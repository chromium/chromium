// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_METADATA_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_METADATA_H_

#include <vector>

#include "base/component_export.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)

#include <jni.h>

#include "base/android/scoped_java_ref.h"

#endif  // defined(OS_ANDROID)

namespace media_session {

// The MediaMetadata is a structure carrying information associated to a
// MediaSession.
struct COMPONENT_EXPORT(MEDIA_SESSION_BASE_CPP) MediaMetadata {
  MediaMetadata();
  ~MediaMetadata();

  MediaMetadata(const MediaMetadata& other);

  bool operator==(const MediaMetadata& other) const;
  bool operator!=(const MediaMetadata& other) const;

#if defined(OS_ANDROID)
  // Creates a Java MediaMetadata instance and returns the JNI ref.
  base::android::ScopedJavaLocalRef<jobject> CreateJavaObject(
      JNIEnv* env) const;
#endif

  // Title associated to the MediaSession.
  base::string16 title;

  // Artist associated to the MediaSession.
  base::string16 artist;

  // Album associated to the MediaSession.
  base::string16 album;

  // The |source_title| is a human readable title for the source of the media
  // session. This could be the name of the app or the name of the site playing
  // media.
  base::string16 source_title;

  // Returns whether |this| contains no metadata.
  bool IsEmpty() const;
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_METADATA_H_
