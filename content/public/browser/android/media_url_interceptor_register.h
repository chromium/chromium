// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_MEDIA_URL_INTERCEPTOR_REGISTER_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_MEDIA_URL_INTERCEPTOR_REGISTER_H_

#include "content/common/content_export.h"

namespace media {
class MediaUrlInterceptor;
}

namespace content {

// Permits embedders to handle custom urls.
CONTENT_EXPORT void RegisterMediaUrlInterceptor(
    media::MediaUrlInterceptor* media_url_interceptor);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_MEDIA_URL_INTERCEPTOR_REGISTER_H_
