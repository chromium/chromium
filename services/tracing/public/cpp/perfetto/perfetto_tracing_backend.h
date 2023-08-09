// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACING_BACKEND_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACING_BACKEND_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing_backend.h"

namespace tracing {
namespace mojom {
class ConsumerHost;
class PerfettoService;
class TracingService;
}  // namespace mojom

class ProducerEndpoint;
class ConsumerEndpoint;

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
  PerfettoTracingBackend();
  ~PerfettoTracingBackend() override;

  void OnProducerConnected(
      mojo::PendingRemote<mojom::PerfettoService> perfetto_service);

  using ConsumerConnectionFactory = mojom::TracingService& (*)();
  void SetConsumerConnectionFactory(ConsumerConnectionFactory,
                                    scoped_refptr<base::SequencedTaskRunner>);

  // perfetto::TracingBackend implementation:
  std::unique_ptr<perfetto::ProducerEndpoint> ConnectProducer(
      const ConnectProducerArgs&) override;
  std::unique_ptr<perfetto::ConsumerEndpoint> ConnectConsumer(
      const ConnectConsumerArgs&) override;

 private:
  void BindProducerConnectionIfNecessary();
  void CreateConsumerConnection(base::WeakPtr<ConsumerEndpoint>);

  SEQUENCE_CHECKER(muxer_sequence_checker_);
  base::Lock task_runner_lock_;
  base::WeakPtr<ProducerEndpoint> producer_endpoint_;
  raw_ptr<perfetto::base::TaskRunner> muxer_task_runner_ = nullptr;
  mojo::PendingRemote<mojom::PerfettoService> perfetto_service_;

  scoped_refptr<base::SequencedTaskRunner> consumer_connection_task_runner_;
  ConsumerConnectionFactory consumer_connection_factory_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_TRACING_BACKEND_H_
