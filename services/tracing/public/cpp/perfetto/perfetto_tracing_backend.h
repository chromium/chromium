// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACING_BACKEND_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACING_BACKEND_H_

#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing_backend.h"

namespace tracing {
namespace mojom {
class ConsumerHost;
class PerfettoService;
}  // namespace mojom

// The Perfetto tracing backend mediates between the Perfetto client library and
// the mojo-based tracing service. It allows any process to emit trace data
// through Perfetto and privileged processes (i.e., the browser) to start
// tracing sessions and read back the resulting data.
//
//      Perfetto         :   Tracing backend     :    Tracing service
//                       :                       :
//                       :                      mojo
//                calls  : .------------------.  :  .--------------.
//             .---------->| ConsumerEndpoint |<--->| ConsumerHost |
//  .--------------.     : `------------------'  :  `--------------'
//  | TracingMuxer |     :                       :
//  `--------------'     : .------------------.  :  .--------------.
//             `---------->| ProducerEndpoint |<--->| ProducerHost |
//                       : `------------------'  :  `--------------'
//                       :                       :
class PerfettoTracingBackend : public perfetto::TracingBackend {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // Called to establish a consumer connection to the tracing service. The
    // callback may be called on an arbitrary sequence.
    virtual void CreateConsumerConnection(
        base::OnceCallback<void(mojo::PendingRemote<mojom::ConsumerHost>)>) = 0;

    // Called to establish a producer connection to the tracing service. The
    // callback may be called on an arbitrary sequence.
    virtual void CreateProducerConnection(
        base::OnceCallback<
            void(mojo::PendingRemote<mojom::PerfettoService>)>) = 0;
  };

  explicit PerfettoTracingBackend(Delegate&);
  ~PerfettoTracingBackend() override;

  // perfetto::TracingBackend implementation:
  std::unique_ptr<perfetto::ProducerEndpoint> ConnectProducer(
      const ConnectProducerArgs&) override;
  std::unique_ptr<perfetto::ConsumerEndpoint> ConnectConsumer(
      const ConnectConsumerArgs&) override;

 private:
  Delegate& delegate_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACING_BACKEND_H_
