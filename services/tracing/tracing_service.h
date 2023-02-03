// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_TRACING_SERVICE_H_
#define SERVICES_TRACING_TRACING_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"

namespace tracing {

class PerfettoService;

class TracingService : public mojom::TracingService {
 public:
  explicit TracingService(PerfettoService* = nullptr);
  explicit TracingService(
      mojo::PendingReceiver<mojom::TracingService> receiver);
  TracingService(const TracingService&) = delete;
  ~TracingService() override;
  TracingService& operator=(const TracingService&) = delete;

  // mojom::TracingService implementation:
  void Initialize(std::vector<mojom::ClientInfoPtr> clients) override;
  void AddClient(mojom::ClientInfoPtr client) override;
#if !BUILDFLAG(IS_NACL) && BUILDFLAG(USE_BLINK)
  void BindConsumerHost(
      mojo::PendingReceiver<mojom::ConsumerHost> receiver) override;
#endif

 private:
  mojo::Receiver<mojom::TracingService> receiver_{this};
  raw_ptr<PerfettoService> perfetto_service_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_TRACING_SERVICE_H_
