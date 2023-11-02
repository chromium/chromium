// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WORKER_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_WORKER_TYPE_H_

// Should correspond to "WorkerType" in enums.xml.
enum class WorkerType {
  kSharedWorker = 0,
  kServiceWorker = 1,
  kMaxValue = kServiceWorker,
};

#endif  // CONTENT_PUBLIC_BROWSER_WORKER_TYPE_H_
