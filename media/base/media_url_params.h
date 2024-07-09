// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_URL_PARAMS_H_
#define MEDIA_BASE_MEDIA_URL_PARAMS_H_

#include "base/containers/flat_map.h"
#include "media/base/media_export.h"
#include "net/cookies/site_for_cookies.h"
#include "net/storage_access_api/status.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {

// Encapsulates the necessary information in order to play media in URL based
// playback (as opposed to stream based).
// See MediaUrlDemuxer and MediaPlayerRenderer.
struct MEDIA_EXPORT MediaUrlParams {
  MediaUrlParams(const GURL& media_url,
                 const net::SiteForCookies& site_for_cookies,
                 const url::Origin& top_frame_origin,
                 net::StorageAccessApiStatus storage_access_api_status,
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
  net::SiteForCookies site_for_cookies;

  // Used to check for cookie content settings.
  url::Origin top_frame_origin;

  // Used to check for cookie access.
  net::StorageAccessApiStatus storage_access_api_status;

  // True when the crossorigin mode is unspecified or set to "use-credentials",
  // false when it's "anonymous".
  //
  // Used by MediaPlayerRenderer when determining whether or not send cookies
  // and credentials to the underlying Android MediaPlayer. Cookies/credentials
  // are retrieved based on whether or not the `media_url` passes the checks
  // initiated in MediaResourceGetter::CheckPolicyForCookies() for the given
  // `site_for_cookies`.
  bool allow_credentials;

  // True when MediaPlayerRenderer has been selected because the media has been
  // detected to be HLS. Used only for metrics.
  bool is_hls;

  // HTTP Request Headers
  base::flat_map<std::string, std::string> headers;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_URL_PARAMS_H_
