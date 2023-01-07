// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/system_producer.h"

namespace tracing {

SystemProducer::SystemProducer(base::tracing::PerfettoTaskRunner* task_runner)
    : PerfettoProducer(task_runner) {}

SystemProducer::~SystemProducer() = default;

bool SystemProducer::IsDummySystemProducerForTesting() {
  return false;
}

}  // namespace tracing
