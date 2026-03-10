// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_TEST_API_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"

namespace content {

class StoragePartitionImpl;

class ServiceWorkerContextWrapperTestApi {
 public:
  explicit ServiceWorkerContextWrapperTestApi(
      ServiceWorkerContextWrapper* wrapper)
      : wrapper_(wrapper) {}

  void set_storage_partition(StoragePartitionImpl* partition) {
    wrapper_->set_storage_partition(partition);
  }

 private:
  raw_ptr<ServiceWorkerContextWrapper> wrapper_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_TEST_API_H_
