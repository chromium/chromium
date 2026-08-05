// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_CLASSIFICATION_REQUEST_TRACKER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_CLASSIFICATION_REQUEST_TRACKER_H_

#include <string>
#include <vector>

#import "base/containers/flat_map.h"
#import "base/functional/callback.h"
#import "components/page_content_annotations/core/page_content_annotations_common.h"
#import "services/metrics/public/cpp/ukm_source_id.h"
#import "url/gurl.h"

// Tracks pending classification requests queued before model load and
// in-flight requests deduplicated by GURL.
class ClassificationRequestTracker {
 public:
  using ClassificationCallback = base::OnceCallback<void(
      const std::vector<page_content_annotations::Category>&)>;

  struct PendingClassification {
    GURL url;
    std::string title;
    std::string page_content;
    ukm::SourceId source_id = ukm::kInvalidSourceId;
    ClassificationCallback callback;
  };

  ClassificationRequestTracker();
  ~ClassificationRequestTracker();

  // Maximum number of pending requests queued while waiting for model load.
  static constexpr size_t kMaxPendingClassifications = 10;

  ClassificationRequestTracker(const ClassificationRequestTracker&) = delete;
  ClassificationRequestTracker& operator=(const ClassificationRequestTracker&) =
      delete;

  // Queues a classification request while the model is loading.
  void EnqueuePending(PendingClassification request);

  // Attaches `callback` to an in-flight classification request for `url`.
  // Returns true if a request for `url` is already in-flight, or false if
  // this is the first request for `url`.
  bool AttachInFlight(const GURL& url, ClassificationCallback callback);

  // Drains and returns all pending requests queued before model load.
  std::vector<PendingClassification> DrainPending();

  // Completes all in-flight callbacks for `url` with `results`.
  void CompleteUrl(
      const GURL& url,
      const std::vector<page_content_annotations::Category>& results);

  // Cancels all pending classifications and in-flight callbacks, running each
  // callback with an empty result vector.
  void CancelAll();

 private:
  std::vector<PendingClassification> pending_classifications_;
  base::flat_map<GURL, std::vector<ClassificationCallback>> pending_callbacks_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_CLASSIFICATION_REQUEST_TRACKER_H_
