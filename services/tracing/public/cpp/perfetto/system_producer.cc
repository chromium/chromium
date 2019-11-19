// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/system_producer.h"

namespace tracing {
SystemProducer::SystemProducer(PerfettoTaskRunner* task_runner)
    : PerfettoProducer(task_runner) {}

bool SystemProducer::IsDummySystemProducerForTesting() {
  return false;
}

}  // namespace tracing
