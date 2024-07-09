// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_RESOURCE_GETTER_H_
#define MEDIA_BASE_ANDROID_MEDIA_RESOURCE_GETTER_H_

#include <stdint.h>

#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "net/storage_access_api/status.h"
#include "url/gurl.h"

namespace net {
class SiteForCookies;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace media {

// Class for asynchronously retrieving resources for a media URL. All callbacks
// are executed on the caller's thread.
class MEDIA_EXPORT MediaResourceGetter {
 public:
  // Callback to get the cookies. Args: cookies string.
  typedef base::OnceCallback<void(const std::string&)> GetCookieCB;

  // Callback to get the auth credentials. Args: username and password.
  typedef base::OnceCallback<void(const std::u16string&, const std::u16string&)>
      GetAuthCredentialsCB;

  // Callback to get the media metadata. Args: duration, width, height, and
  // whether the information is retrieved successfully.
  typedef base::OnceCallback<void(base::TimeDelta, int, int, bool)>
      ExtractMediaMetadataCB;
  virtual ~MediaResourceGetter();

  // Method for getting the auth credentials for a URL.
  virtual void GetAuthCredentials(const GURL& url,
                                  GetAuthCredentialsCB callback) = 0;

  // Method for getting the cookies for a given URL.
  virtual void GetCookies(const GURL& url,
                          const net::SiteForCookies& site_for_cookies,
                          const url::Origin& top_frame_origin,
                          net::StorageAccessApiStatus storage_access_api_status,
                          GetCookieCB callback) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_RESOURCE_GETTER_H_
