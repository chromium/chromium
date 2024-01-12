// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPT_READER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPT_READER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/cpp/net_adapters.h"

namespace content {

// Reads a single service worker script from installed script storage. This acts
// as a wrapper of ServiceWorkerResourceReader and converts script code cache
// (metadata) from a BigBuffer to a mojo data pipe.
class ServiceWorkerInstalledScriptReader {
 public:
  // Do not change the order. This is used for UMA.
  enum class FinishedReason {
    kNotFinished = 0,
    kSuccess = 1,
    kNoResponseHeadError = 2,
    kCreateDataPipeError = 3,
    kConnectionError = 4,
    kResponseReaderError = 5,
    kMetaDataSenderError = 6,
    kNoContextError = 7,
    // Add a new type here, then update kMaxValue and enums.xml.
    kMaxValue = kNoContextError,
  };

  // Receives the read data.
  class Client {
   public:
    virtual void OnStarted(
        network::mojom::URLResponseHeadPtr response_head,
        std::optional<mojo_base::BigBuffer> metadata,
        mojo::ScopedDataPipeConsumerHandle body_handle,
        mojo::ScopedDataPipeConsumerHandle meta_data_handle) = 0;
    // Called after both body and metadata have finished being written to the
    // data pipes, or called immediately if an error occurred.
    virtual void OnFinished(FinishedReason reason) = 0;
  };

  // Uses |reader| to read an installed service worker script, and sends it to
  // |client|. |client| must outlive this.
  ServiceWorkerInstalledScriptReader(
      mojo::Remote<storage::mojom::ServiceWorkerResourceReader> reader,
      Client* client);
  ~ServiceWorkerInstalledScriptReader();

  // Starts reading the script.
  void Start();

 private:
  class MetaDataSender;
  void OnReadResponseHeadComplete(
      int result,
      network::mojom::URLResponseHeadPtr response_head,
      std::optional<mojo_base::BigBuffer> metadata);
  void OnReadDataPrepared(
      network::mojom::URLResponseHeadPtr response_head,
      std::optional<mojo_base::BigBuffer> metadata,
      mojo::ScopedDataPipeConsumerHandle body_consumer_handle);
  void OnMetaDataSent(bool success);
  void OnReaderDisconnected();
  void CompleteSendIfNeeded(FinishedReason reason);
  bool WasMetadataWritten() const { return !meta_data_sender_; }
  bool WasBodyWritten() const { return was_body_written_; }

  base::WeakPtr<ServiceWorkerInstalledScriptReader> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void OnComplete(int32_t status);

  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> reader_;
  // |client_| must outlive this.
  raw_ptr<Client> client_;

  // For meta data.
  std::unique_ptr<MetaDataSender> meta_data_sender_;

  // For body.
  // Initialized to max uint64_t to default to reading until EOF, but updated
  // to an expected body size in OnReadInfoCompete().
  uint64_t body_size_ = std::numeric_limits<uint64_t>::max();
  bool was_body_written_ = false;

  base::WeakPtrFactory<ServiceWorkerInstalledScriptReader> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPT_READER_H_
