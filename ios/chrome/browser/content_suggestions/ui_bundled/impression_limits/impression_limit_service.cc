// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/impression_limit_service.h"

#import "components/history/core/browser/history_service.h"

ImpressionLimitService::ImpressionLimitService(
    history::HistoryService* history_service)
    : history_service_(history_service) {
  DCHECK(history_service_);
  history_service_observation_.Observe(history_service_.get());
}

ImpressionLimitService::~ImpressionLimitService() = default;

void ImpressionLimitService::Shutdown() {
  if (history_service_) {
    history_service_observation_.Reset();
  }
  history_service_ = nullptr;
}

void ImpressionLimitService::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // TODO(crbug.com/407527797) Remove impressions for a URL
  // when the URL is deleted from history.
}
