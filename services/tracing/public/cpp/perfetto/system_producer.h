// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SYSTEM_PRODUCER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SYSTEM_PRODUCER_H_

#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/producer.h"

namespace tracing {

class COMPONENT_EXPORT(TRACING_CPP) SystemProducer : public PerfettoProducer,
                                                     public perfetto::Producer {
 public:
  SystemProducer(PerfettoTaskRunner* task_runner);
  // Since Chrome does not support concurrent tracing sessions, and system
  // tracing is always lower priority than human or DevTools initiated tracing,
  // all system producers must be able to disconnect and stop tracing.
  //
  // Should call |on_disconnect_complete| after any active tracing sessions was
  // terminated (i.e. all DataSources have been stopped).
  virtual void DisconnectWithReply(
      base::OnceClosure on_disconnect_complete) = 0;

  virtual bool IsDummySystemProducerForTesting();
  virtual void ResetSequenceForTesting() = 0;
};
}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SYSTEM_PRODUCER_H_
