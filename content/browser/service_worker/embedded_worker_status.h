// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_STATUS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_STATUS_H_

namespace content {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EmbeddedWorkerStatus {
  STOPPED = 0,
  STARTING = 1,
  RUNNING = 2,
  STOPPING = 3,
  kMaxValue = STOPPING,
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_STATUS_H_
