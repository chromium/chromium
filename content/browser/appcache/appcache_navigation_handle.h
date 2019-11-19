// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_NAVIGATION_HANDLE_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_NAVIGATION_HANDLE_H_

#include <memory>
#include "base/macros.h"
#include "base/unguessable_token.h"

namespace content {

class AppCacheHost;
class ChromeAppCacheService;

// This class is used to manage the lifetime of AppCacheHosts created during
// navigation. This is a UI thread class.
//
// The lifetime of the AppCacheNavigationHandle and the AppCacheHost are the
// following :
//   1) We create a AppCacheNavigationHandle and precreated AppCacheHost on the
//      UI thread with a app cache host id of -1.
//
//   2) When the navigation is ready to commit, the NavigationRequest will
//      update the CommitNavigationParams based on the id from the
//      AppCacheNavigationHandle.
//
//   3) The commit leads to AppCache registrations happening from the renderer.
//      This is via the AppCacheBackend.RegisterHost mojo call. The
//      AppCacheBackendImpl class which handles these calls will be informed
//      about these hosts when the navigation commits. It will ignore the
//      host registrations as they have already been registered. The
//      ownership of the precreated AppCacheHost is passed from the
//      AppCacheNavigationHandle to the AppCacheBackendImpl.
//
//   4) Meanwhile, RenderFrameHostImpl takes ownership of
//      AppCacheNavigationHandle once navigation commits, so that the precreated
//      AppCacheHost is not destroyed before IPC above reaches AppCacheBackend.
//
//   5) When the next navigation commits, previous AppCacheNavigationHandle is
//      destroyed.

class AppCacheNavigationHandle {
 public:
  AppCacheNavigationHandle(ChromeAppCacheService* appcache_service,
                           int process_id);
  ~AppCacheNavigationHandle();

  const base::UnguessableToken& appcache_host_id() const {
    return appcache_host_id_;
  }

  // Returns the precreated AppCacheHost pointer. Ownership of the host is
  // released here.
  static std::unique_ptr<AppCacheHost> TakePrecreatedHost(
      const base::UnguessableToken& host_id);

  // Returns the raw AppCacheHost pointer. Ownership remains with this class.
  AppCacheHost* host() { return precreated_host_.get(); }

 private:
  const base::UnguessableToken appcache_host_id_;
  std::unique_ptr<AppCacheHost> precreated_host_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheNavigationHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_NAVIGATION_HANDLE_H_
