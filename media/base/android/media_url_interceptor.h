// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_URL_INTERCEPTOR_H_
#define MEDIA_BASE_ANDROID_MEDIA_URL_INTERCEPTOR_H_

#include <stdint.h>

#include <string>

#include "base/android/jni_android.h"
#include "media/base/media_export.h"

namespace media {

// Interceptor for content embedders to handle custom media urls
// and translate them into files containing media.
class MEDIA_EXPORT MediaUrlInterceptor {
 public:
  virtual ~MediaUrlInterceptor() {}

  // Returns true if the embedder has intercepted the url and
  // false otherwise.
  // Output arguments (only when the url has been intercepted):
  // - |fd|: file descriptor to the file containing the media element.
  // - |offset|: offset in bytes from the start of the file to the
  //             media element.
  // - |size|: size in bytes of the media element.
  virtual bool Intercept(const std::string& url,
                         int* fd,
                         int64_t* offset,
                         int64_t* size) const = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_URL_INTERCEPTOR_H_
