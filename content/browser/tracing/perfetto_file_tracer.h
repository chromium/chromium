// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_PERFETTO_FILE_TRACER_H_
#define CONTENT_BROWSER_TRACING_PERFETTO_FILE_TRACER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "base/threading/sequence_bound.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"

namespace content {

namespace {
class BackgroundDrainer;
}  // namespace

// This is currently only used for tracing startup using Perfetto
// as the backend, rather than TraceLog. It will directly stream
// protos to a file specified with the '--perfetto-output-file'
// switch.
class PerfettoFileTracer : public tracing::mojom::TracingSessionClient {
 public:
  PerfettoFileTracer();
  ~PerfettoFileTracer() override;

  static bool ShouldEnable();

  // tracing::mojom::TracingSessionClient implementation:
  void OnTracingEnabled() override;
  void OnTracingDisabled(bool tracing_succeeded) override;

  bool is_finished_for_testing() const { return !background_drainer_; }

 private:
  void OnNoMorePackets(bool queued_after_disable);
  void ReadBuffers();

  void OnTracingSessionEnded();

  base::SequenceBound<BackgroundDrainer> background_drainer_;
  mojo::Receiver<tracing::mojom::TracingSessionClient> receiver_{this};
  mojo::Remote<tracing::mojom::TracingSessionHost> tracing_session_host_;
  mojo::Remote<tracing::mojom::ConsumerHost> consumer_host_;
  bool has_been_disabled_ = false;

  base::WeakPtrFactory<PerfettoFileTracer> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(PerfettoFileTracer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_PERFETTO_FILE_TRACER_H_
