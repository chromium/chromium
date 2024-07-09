// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_RESOURCE_GETTER_IMPL_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_RESOURCE_GETTER_IMPL_H_

#include <jni.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/android/media_resource_getter.h"
#include "media/base/android/media_url_interceptor.h"
#include "net/base/auth.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/site_for_cookies.h"
#include "net/storage_access_api/status.h"

namespace content {

class BrowserContext;
class ResourceContext;

// This class implements media::MediaResourceGetter to retrieve resources
// asynchronously on the UI thread.
class MediaResourceGetterImpl : public media::MediaResourceGetter {
 public:
  // Construct a MediaResourceGetterImpl object. `browser_context` and
  // `render_process_id` are passed to retrieve the CookieStore.
  MediaResourceGetterImpl(BrowserContext* browser_context,
                          int render_process_id,
                          int render_frame_id);

  MediaResourceGetterImpl(const MediaResourceGetterImpl&) = delete;
  MediaResourceGetterImpl& operator=(const MediaResourceGetterImpl&) = delete;

  ~MediaResourceGetterImpl() override;

  // media::MediaResourceGetter implementation.
  // Must be called on the UI thread.
  void GetAuthCredentials(const GURL& url,
                          GetAuthCredentialsCB callback) override;
  void GetCookies(const GURL& url,
                  const net::SiteForCookies& site_for_cookies,
                  const url::Origin& top_frame_origin,
                  net::StorageAccessApiStatus storage_access_api_status,
                  GetCookieCB callback) override;

 private:
  // Called when GetAuthCredentials() finishes.
  void GetAuthCredentialsCallback(
      GetAuthCredentialsCB callback,
      const std::optional<net::AuthCredentials>& credentials);

  // BrowserContext to retrieve URLRequestContext and ResourceContext.
  raw_ptr<BrowserContext> browser_context_;

  // Render process id, used to check whether the process can access cookies.
  int render_process_id_;

  // Render frame id, used to check tab specific cookie policy.
  int render_frame_id_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaResourceGetterImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_RESOURCE_GETTER_IMPL_H_
