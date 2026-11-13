// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/suspicious_site_history_observer_service.h"

#import <utility>

#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"

SuspiciousSiteHistoryObserverService::SuspiciousSiteHistoryObserverService(
    history::HistoryService* history_service,
    scoped_refptr<HostContentSettingsMap> host_content_settings_map)
    : host_content_settings_map_(std::move(host_content_settings_map)) {
  // `history_service` may be null in test profiles or if history database
  // initialization fails.
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
}

SuspiciousSiteHistoryObserverService::~SuspiciousSiteHistoryObserverService() =
    default;

#pragma mark - KeyedService

void SuspiciousSiteHistoryObserverService::Shutdown() {
  history_service_observation_.Reset();
  host_content_settings_map_.reset();
}

#pragma mark - history::HistoryServiceObserver

void SuspiciousSiteHistoryObserverService::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (!host_content_settings_map_) {
    // `Shutdown()` may have already released `host_content_settings_map_`.
    return;
  }
  safe_browsing::SuspiciousSiteWarningAllowlist(host_content_settings_map_)
      .OnHistoryDeletions(deletion_info);
}

void SuspiciousSiteHistoryObserverService::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observation_.Reset();
}
