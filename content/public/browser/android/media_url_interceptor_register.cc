// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/media_url_interceptor_register.h"

#include "content/browser/media/android/media_player_renderer.h"

namespace content {

void RegisterMediaUrlInterceptor(
    media::MediaUrlInterceptor* media_url_interceptor) {
  content::MediaPlayerRenderer::RegisterMediaUrlInterceptor(
      media_url_interceptor);
}

}  // namespace content
