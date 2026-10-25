// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SITE_SHARD_H_
#define CONTENT_BROWSER_SITE_SHARD_H_

#include <ostream>
#include <string_view>

#include "content/common/content_export.h"

namespace content {

// Sites that don't require a dedicated process are split into "shards", and
// sites in different shards never share a renderer process. Shards only
// apply to processes that are not locked to a site (e.g. processes for default
// SiteInstances); sites that require a dedicated process are always in the
// default shard. See BrowserContext::SetSitesForEmbedderShard() for details.
//
// TODO(crbug.com/576925194): Determine the correct SiteShard value for sites
// with dedicated processes.
enum class SiteShard {
  // The shard containing all sites that are not in the embedder shard.
  kDefault,
  // The shard containing the sites passed to
  // BrowserContext::SetSitesForEmbedderShard().
  kEmbedderSites,
};

// Returns a human-readable name for `shard`, for logging and debugging.
CONTENT_EXPORT std::string_view SiteShardToString(SiteShard shard);

CONTENT_EXPORT std::ostream& operator<<(std::ostream& out, SiteShard shard);

}  // namespace content

#endif  // CONTENT_BROWSER_SITE_SHARD_H_
