// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_DISABLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_DISABLE_H_

#include "content/common/content_export.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

class CONTENT_EXPORT BackForwardCacheDisable {
 public:
  // Reasons to disable BackForwardCache for this frame for content features.
  enum class DisabledReasonId : BackForwardCache::DisabledReasonType {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused. kMaxValue is not defined because
    // this enum is not logged directly as an enum (see
    // BackForwardCache::DisabledSource).
    kUnknown = 0,
    // kMediaSessionImplOnServiceCreated = 1, Removed after implementing
    // MediaSessionImplOnServiceCreated support in back/forward cache.
    kSecurityHandler = 2,
    kWebAuthenticationAPI = 3,
    kFileChooser = 4,
    kSerial = 5,
    // kFileSystemAccess = 6, Removed. See https://crbug.com/1259861.
    kMediaDevicesDispatcherHost = 7,
    kWebBluetooth = 8,
    kWebUSB = 9,

    // MediaSession's playback state is changed (crbug.com/1177661).
    // kMediaSession = 10 Removed after implementing support

    // MediaSession's service is used (crbug.com/1243599).
    kMediaSessionService = 11,

    // kMediaPlay = 12, Removed after allowing media play (crbug.com/1246240).

    // TODO(crbug.com/40805561): Screen readers do not recognize a navigation
    // when the page is served from bfcache.
    kScreenReader = 13,

    // Documents that are cleared for discard should not be BFCached.
    kDiscarded = 14,

    // New reasons should be accompanied by a comment as to why BackForwardCache
    // cannot be used in this case and a link to a bug to fix that if it is
    // fixable.
    // Any updates here should be reflected in
    // tools/metrics/histograms/enums.xml
  };

  // Constructs a content-specific DisabledReason
  static BackForwardCache::DisabledReason DisabledReason(
      DisabledReasonId reason_id);
};
}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_DISABLE_H_
