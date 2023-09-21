// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_BACKGROUND_SYNC_CONTEXT_H_
#define CONTENT_TEST_TEST_BACKGROUND_SYNC_CONTEXT_H_

#include "content/browser/background_sync/background_sync_context_impl.h"

namespace content {

// A BackgroundSyncContextImpl for use in unit tests, primarily to create a test
// BackgroundSyncManager.
class TestBackgroundSyncContext : public BackgroundSyncContextImpl {
 public:
  TestBackgroundSyncContext() = default;

  TestBackgroundSyncContext(const TestBackgroundSyncContext&) = delete;
  TestBackgroundSyncContext& operator=(const TestBackgroundSyncContext&) =
      delete;

 protected:
  ~TestBackgroundSyncContext() override = default;

  // BackgroundSyncContextImpl:
  void CreateBackgroundSyncManager(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      DevToolsBackgroundServicesContextImpl& devtools_context) override;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_BACKGROUND_SYNC_CONTEXT_H_
