// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_shard.h"

#include "base/notreached.h"

namespace content {

std::string_view SiteShardToString(SiteShard shard) {
  switch (shard) {
    case SiteShard::kDefault:
      return "default";
    case SiteShard::kEmbedderSites:
      return "embedder-sites";
  }
  NOTREACHED();
}

std::ostream& operator<<(std::ostream& out, SiteShard shard) {
  return out << SiteShardToString(shard);
}

}  // namespace content
