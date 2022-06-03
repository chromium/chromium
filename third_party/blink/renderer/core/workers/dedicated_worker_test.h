// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_DEDICATED_WORKER_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_DEDICATED_WORKER_TEST_H_

#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class DedicatedWorkerThreadForTest;
class DedicatedWorkerMessagingProxyForTest;

class DedicatedWorkerTest : public PageTestBase {
 public:
  DedicatedWorkerTest() = default;

  void SetUp() override;

  void TearDown() override;

  void DispatchMessageEvent();

  DedicatedWorkerMessagingProxyForTest* WorkerMessagingProxy();

  DedicatedWorkerThreadForTest* GetWorkerThread();

  void StartWorker();
  void EvaluateClassicScript(const String& source_code);
  void WaitUntilWorkerIsRunning();

 private:
  Persistent<DedicatedWorkerMessagingProxyForTest> worker_messaging_proxy_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_DEDICATED_WORKER_TEST_H_
