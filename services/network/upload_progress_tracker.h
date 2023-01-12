// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_UPLOAD_PROGRESS_TRACKER_H_
#define SERVICES_NETWORK_UPLOAD_PROGRESS_TRACKER_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/upload_progress.h"

namespace base {
class Location;
}

namespace net {
class URLRequest;
}

namespace network {

// UploadProgressTracker watches the upload progress of a URL loading, and sends
// the progress to the client in a suitable granularity and frequency.
class COMPONENT_EXPORT(NETWORK_SERVICE) UploadProgressTracker {
 public:
  using UploadProgressReportCallback =
      base::RepeatingCallback<void(const net::UploadProgress&)>;

  UploadProgressTracker(const base::Location& location,
                        UploadProgressReportCallback report_progress,
                        net::URLRequest* request,
                        scoped_refptr<base::SequencedTaskRunner> task_runner =
                            base::SequencedTaskRunner::GetCurrentDefault());

  UploadProgressTracker(const UploadProgressTracker&) = delete;
  UploadProgressTracker& operator=(const UploadProgressTracker&) = delete;

  virtual ~UploadProgressTracker();

  void OnAckReceived();
  void OnUploadCompleted();

  static base::TimeDelta GetUploadProgressIntervalForTesting();

 private:
  // Overridden by tests to use a fake time and progress.
  virtual base::TimeTicks GetCurrentTime() const;
  virtual net::UploadProgress GetUploadProgress() const;

  void ReportUploadProgressIfNeeded();

  raw_ptr<net::URLRequest> request_;  // Not owned.

  uint64_t last_upload_position_ = 0;
  bool waiting_for_upload_progress_ack_ = false;
  base::TimeTicks last_upload_ticks_;
  base::RepeatingTimer progress_timer_;

  UploadProgressReportCallback report_progress_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_UPLOAD_PROGRESS_TRACKER_H_
