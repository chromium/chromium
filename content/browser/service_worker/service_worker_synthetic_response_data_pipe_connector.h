// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SYNTHETIC_RESPONSE_DATA_PIPE_CONNECTOR_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SYNTHETIC_RESPONSE_DATA_PIPE_CONNECTOR_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace content {

// A class to read data from a data pipe and write it to another data pipe.
// This is used for ServiceWorkerSyntheticResponse, which sends a network
// request and passes the response body data to the existing
// DataPipeConsumerHandle.
class CONTENT_EXPORT ServiceWorkerSyntheticResponseDataPipeConnector
    : public mojo::DataPipeDrainer::Client {
 public:
  explicit ServiceWorkerSyntheticResponseDataPipeConnector(
      mojo::ScopedDataPipeConsumerHandle consumer_handle);

  ServiceWorkerSyntheticResponseDataPipeConnector(
      const ServiceWorkerSyntheticResponseDataPipeConnector&) = delete;
  ServiceWorkerSyntheticResponseDataPipeConnector& operator=(
      const ServiceWorkerSyntheticResponseDataPipeConnector&) = delete;

  ~ServiceWorkerSyntheticResponseDataPipeConnector() override;

  // Starts writing the buffered data into `producer_handle`. Run the
  // `on_finished` after all buffered data is written or a write failure
  // occurs.
  void Transfer(mojo::ScopedDataPipeProducerHandle producer_handle,
                base::OnceClosure on_finished);

 private:
  // mojo::DataPipeDrainer::Client overrides:
  void OnDataAvailable(base::span<const uint8_t> data) override;
  void OnDataComplete() override;

  void OnWriteAvailable(MojoResult result,
                        const mojo::HandleSignalsState& state);

  void Finish();

  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<mojo::SimpleWatcher> producer_handle_watcher_;
  base::OnceClosure on_complete_;
  // TODO(crbug.com/352578800): Consider having a maximum limit for this
  // buffer.
  std::vector<uint8_t> internal_buffer_;
  size_t write_position_ = 0;
  bool data_complete_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ServiceWorkerSyntheticResponseDataPipeConnector>
      weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SYNTHETIC_RESPONSE_DATA_PIPE_CONNECTOR_H_
