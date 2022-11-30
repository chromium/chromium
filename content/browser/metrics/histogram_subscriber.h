// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_METRICS_HISTOGRAM_SUBSCRIBER_H_
#define CONTENT_BROWSER_METRICS_HISTOGRAM_SUBSCRIBER_H_

#include <string>
#include <vector>

namespace content {

// Objects interested in receiving histograms derive from HistogramSubscriber.
class HistogramSubscriber {
 public:
  virtual ~HistogramSubscriber() {}

  // Send number of pending processes to subscriber. |end| is set to true if it
  // is the last time. This is called on the UI thread.
  virtual void OnPendingProcesses(int sequence_number,
                                  int pending_processes,
                                  bool end) = 0;

  // Send |histogram| back to subscriber.
  // This is called on the UI thread.
  virtual void OnHistogramDataCollected(
      int sequence_number,
      const std::vector<std::string>& pickled_histograms) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_METRICS_HISTOGRAM_SUBSCRIBER_H_
