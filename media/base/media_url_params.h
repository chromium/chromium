// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_URL_PARAMS_H_
#define MEDIA_BASE_MEDIA_URL_PARAMS_H_

#include "media/base/media_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {

// Encapsulates the necessary information in order to play media in URL based
// playback (as opposed to stream based).
// See MediaUrlDemuxer and MediaPlayerRenderer.
struct MEDIA_EXPORT MediaUrlParams {
  MediaUrlParams(GURL media_url,
                 GURL site_for_cookies,
                 url::Origin top_frame_origin,
                 bool allow_credentials,
                 bool is_hls);
  MediaUrlParams(const MediaUrlParams& other);
  ~MediaUrlParams();

  // URL of the media to be played.
  GURL media_url;

  // Used to play media in authenticated scenarios.
  // In the MediaPlayerRenderer case, it will ultimately be used in
  // MediaResourceGetterTask::CheckPolicyForCookies, to limit the scope of the
  // cookies that the MediaPlayerRenderer has access to.
  GURL site_for_cookies;

  // Used to check for cookie content settings.
  url::Origin top_frame_origin;

  // True when the crossorigin mode is unspecified or set to "use-credentials",
  // false when it's "anonymous".
  //
  // Used by MediaPlayerRenderer when determining whether or not send cookies
  // and credentials to the underlying Android MediaPlayer. Cookies/credentials
  // are retrieved based on whether or not the |media_url| passes the checks
  // initiated in MediaResourceGetter::CheckPolicyForCookies() for the given
  // |site_for_cookies|.
  bool allow_credentials;

  // True when MediaPlayerRenderer has been selected because the media has been
  // detected to be HLS. Used only for metrics.
  bool is_hls;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_URL_PARAMS_H_
