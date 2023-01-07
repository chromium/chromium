// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACED_PROCESS_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACED_PROCESS_H_

#include "services/tracing/public/mojom/system_tracing_service.mojom.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"

namespace tracing {

class TracedProcess {
 public:
  static void ResetTracedProcessReceiver();
  static void OnTracedProcessRequest(
      mojo::PendingReceiver<mojom::TracedProcess> receiver);
  static void EnableSystemTracingService(
      mojo::PendingRemote<mojom::SystemTracingService> remote);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACED_PROCESS_H_
