// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_METADATA_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_METADATA_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "build/build_config.h"
#include "services/media_session/public/cpp/chapter_information.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)

#include <jni.h>

#include "base/android/scoped_java_ref.h"

#endif  // BUILDFLAG(IS_ANDROID)

namespace media_session {

// The MediaMetadata is a structure carrying information associated to a
// MediaSession.
struct COMPONENT_EXPORT(MEDIA_SESSION_BASE_CPP) MediaMetadata {
  MediaMetadata();
  ~MediaMetadata();

  MediaMetadata(const MediaMetadata& other);

  bool operator==(const MediaMetadata& other) const;
  bool operator!=(const MediaMetadata& other) const;

#if BUILDFLAG(IS_ANDROID)
  // Creates a Java MediaMetadata instance and returns the JNI ref.
  base::android::ScopedJavaLocalRef<jobject> CreateJavaObject(
      JNIEnv* env) const;
#endif

  // Title associated to the MediaSession.
  std::u16string title;

  // Artist associated to the MediaSession.
  std::u16string artist;

  // Album associated to the MediaSession.
  std::u16string album;

  // The |source_title| is a human readable title for the source of the media
  // session. This could be the name of the app or the name of the site playing
  // media.
  std::u16string source_title;

  // The chapters associated to the MediaSession.
  std::vector<ChapterInformation> chapters;

  // Returns whether |this| contains no metadata.
  bool IsEmpty() const;
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_METADATA_H_
