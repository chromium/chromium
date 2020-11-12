// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_REGISTRY_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_REGISTRY_H_

#include <map>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class PrerenderHost;

// Prerender2:
// PrerenderHostRegistry manages running prerender hosts and provides the host
// to navigation code for activating prerendered contents. This is created and
// owned by StoragePartitionImpl.
//
// TODO(https://crbug.com/1132746): Once the Prerender2 is migrated to the
// MPArch, it would be more natural to make PrerenderHostRegistry scoped to
// WebContents, that is, WebContents will own this.
class CONTENT_EXPORT PrerenderHostRegistry {
 public:
  PrerenderHostRegistry();
  ~PrerenderHostRegistry();

  PrerenderHostRegistry(const PrerenderHostRegistry&) = delete;
  PrerenderHostRegistry& operator=(const PrerenderHostRegistry&) = delete;
  PrerenderHostRegistry(PrerenderHostRegistry&&) = delete;
  PrerenderHostRegistry& operator=(PrerenderHostRegistry&&) = delete;

  // Registers the host for `prerendering_url`.
  void RegisterHost(const GURL& prerendering_url,
                    std::unique_ptr<PrerenderHost> prerender_host);

  // Destroys the host registered for `prerendering_url`.
  void AbandonHost(const GURL& prerendering_url);

  // Selects the host for navigation to `url`. Returns nullptr if it's not found
  // or not ready for activation yet.
  std::unique_ptr<PrerenderHost> SelectForNavigation(const GURL& url);

  // Returns a prerender host for `prerendering_url`. Returns nullptr if the URL
  // doesn't match any prerender host.
  PrerenderHost* FindHostByUrlForTesting(const GURL& prerendering_url);

 private:
  // TODO(https://crbug.com/1132746): Expire prerendered contents if they are
  // not used for a while.
  std::map<GURL, std::unique_ptr<PrerenderHost>> prerender_host_by_url_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_REGISTRY_H_
