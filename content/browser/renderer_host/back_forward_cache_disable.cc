// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/public/browser/back_forward_cache.h"

namespace content {

std::string ReasonIdToString(
    BackForwardCacheDisable::DisabledReasonId reason_id) {
  switch (reason_id) {
    case BackForwardCacheDisable::DisabledReasonId::kUnknown:
      return "Unknown";
    case BackForwardCacheDisable::DisabledReasonId::kSecurityHandler:
      return "content::protocol::SecurityHandler";
    case BackForwardCacheDisable::DisabledReasonId::kWebAuthenticationAPI:
      return "WebAuthenticationAPI";
    case BackForwardCacheDisable::DisabledReasonId::kFileChooser:
      return "FileChooser";
    case BackForwardCacheDisable::DisabledReasonId::kSerial:
      return "Serial";
    case BackForwardCacheDisable::DisabledReasonId::kMediaDevicesDispatcherHost:
      return "MediaDevicesDispatcherHost";
    case BackForwardCacheDisable::DisabledReasonId::kWebBluetooth:
      return "WebBluetooth";
    case BackForwardCacheDisable::DisabledReasonId::kWebUSB:
      return "WebUSB";
    case BackForwardCacheDisable::DisabledReasonId::kMediaSessionService:
      return "MediaSessionService";
    case BackForwardCacheDisable::DisabledReasonId::kScreenReader:
      return "ScreenReader";
    case BackForwardCacheDisable::DisabledReasonId::kDiscarded:
      return "Discarded";
  }
}

// static
BackForwardCache::DisabledReason BackForwardCacheDisable::DisabledReason(
    DisabledReasonId reason_id) {
  return BackForwardCache::DisabledReason(
      content::BackForwardCache::DisabledSource::kContent,
      static_cast<BackForwardCache::DisabledReasonType>(reason_id),
      ReasonIdToString(reason_id), /*context=*/"", ReasonIdToString(reason_id));
}
}  // namespace content
