// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/classification_request_tracker.h"

#import <utility>

ClassificationRequestTracker::ClassificationRequestTracker() = default;

ClassificationRequestTracker::~ClassificationRequestTracker() = default;

void ClassificationRequestTracker::EnqueuePending(
    PendingClassification request) {
  if (pending_classifications_.size() >= kMaxPendingClassifications) {
    PendingClassification oldest = std::move(pending_classifications_.front());
    pending_classifications_.erase(pending_classifications_.begin());
    if (oldest.callback) {
      std::move(oldest.callback).Run({});
    }
  }
  pending_classifications_.push_back(std::move(request));
}

bool ClassificationRequestTracker::AttachInFlight(
    const GURL& url,
    ClassificationCallback callback) {
  auto callbacks_it = pending_callbacks_.find(url);
  bool already_in_flight = (callbacks_it != pending_callbacks_.end());
  pending_callbacks_[url].push_back(std::move(callback));
  return already_in_flight;
}

std::vector<ClassificationRequestTracker::PendingClassification>
ClassificationRequestTracker::DrainPending() {
  std::vector<PendingClassification> queued =
      std::move(pending_classifications_);
  pending_classifications_.clear();
  return queued;
}

void ClassificationRequestTracker::CompleteUrl(
    const GURL& url,
    const std::vector<page_content_annotations::Category>& results) {
  auto it = pending_callbacks_.find(url);
  if (it == pending_callbacks_.end()) {
    return;
  }
  std::vector<ClassificationCallback> callbacks = std::move(it->second);
  pending_callbacks_.erase(it);
  for (auto& cb : callbacks) {
    std::move(cb).Run(results);
  }
}

void ClassificationRequestTracker::CancelAll() {
  auto pending_callbacks = std::move(pending_callbacks_);
  pending_callbacks_.clear();
  for (auto& [url, callbacks] : pending_callbacks) {
    for (auto& cb : callbacks) {
      std::move(cb).Run({});
    }
  }

  auto pending_classifications = std::move(pending_classifications_);
  pending_classifications_.clear();
  for (auto& req : pending_classifications) {
    std::move(req.callback).Run({});
  }
}
