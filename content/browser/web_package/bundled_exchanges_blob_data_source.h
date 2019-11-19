// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_BLOB_DATA_SOURCE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_BLOB_DATA_SOURCE_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "services/data_decoder/public/mojom/bundled_exchanges_parser.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace storage {
class BlobBuilderFromStream;
class BlobDataHandle;
}  // namespace storage

namespace content {

// This class is used to read partial body data of a web bundle response from
// server. Currently this class can't read any partial body data before
// receiving the whole body. TODO(crbug/1018640): Support progressive loading.
class CONTENT_EXPORT BundledExchangesBlobDataSource {
 public:
  using CompletionCallback = base::OnceCallback<void(net::Error net_error)>;

  // This class keeps |endpoints| to keep the ongoing network request.
  BundledExchangesBlobDataSource(
      int64_t content_length,
      mojo::ScopedDataPipeConsumerHandle outer_response_body,
      network::mojom::URLLoaderClientEndpointsPtr endpoints,
      BrowserContext::BlobContextGetter blob_context_getter,
      mojo::PendingReceiver<data_decoder::mojom::BundleDataSource>
          pending_receiver);
  ~BundledExchangesBlobDataSource();

  void ReadToDataPipe(uint64_t offset,
                      uint64_t length,
                      mojo::ScopedDataPipeProducerHandle producer_handle,
                      CompletionCallback callback);

 private:
  // This class lives on the IO thread.
  class BlobDataSourceCore : public data_decoder::mojom::BundleDataSource {
   public:
    BlobDataSourceCore(
        int64_t content_length,
        network::mojom::URLLoaderClientEndpointsPtr endpoints,
        BrowserContext::BlobContextGetter blob_context_getter,
        mojo::PendingReceiver<data_decoder::mojom::BundleDataSource>
            pending_receiver);
    ~BlobDataSourceCore() override;

    void Start(mojo::ScopedDataPipeConsumerHandle outer_response_body);

    void ReadToDataPipe(uint64_t offset,
                        uint64_t length,
                        mojo::ScopedDataPipeProducerHandle producer_handle,
                        CompletionCallback callback);

    base::WeakPtr<BlobDataSourceCore> GetWeakPtr();

   private:
    // Implements data_decoder::mojom::BundleDataSource.
    void GetSize(GetSizeCallback callback) override;
    void Read(uint64_t offset, uint64_t length, ReadCallback callback) override;

    void StreamingBlobDone(storage::BlobBuilderFromStream* builder,
                           std::unique_ptr<storage::BlobDataHandle> result);
    void WaitForBlob(base::OnceClosure closure);

    void OnBlobReadyForRead(uint64_t offset,
                            uint64_t length,
                            ReadCallback callback);
    void OnBlobReadyForReadToDataPipe(
        uint64_t offset,
        uint64_t length,
        mojo::ScopedDataPipeProducerHandle producer_handle,
        CompletionCallback callback);

    const int64_t content_length_;
    // Used to keep the ongoing network request.
    network::mojom::URLLoaderClientEndpointsPtr endpoints_;
    mojo::Receiver<data_decoder::mojom::BundleDataSource> receiver_;
    std::unique_ptr<storage::BlobBuilderFromStream> blob_builder_from_stream_;

    std::unique_ptr<storage::BlobDataHandle> blob_;

    // Used to wait StreamingBlobDone() to be called.
    std::vector<base::OnceClosure> pending_get_blob_tasks_;

    base::WeakPtrFactory<BlobDataSourceCore> weak_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(BlobDataSourceCore);
  };

  static void CreateCoreOnIO(
      base::WeakPtr<BundledExchangesBlobDataSource> weak_ptr,
      int64_t content_length,
      mojo::ScopedDataPipeConsumerHandle outer_response_body,
      network::mojom::URLLoaderClientEndpointsPtr endpoints,
      BrowserContext::BlobContextGetter blob_context_getter,
      mojo::PendingReceiver<data_decoder::mojom::BundleDataSource>
          pending_receiver);

  static void SetCoreOnUI(
      base::WeakPtr<BundledExchangesBlobDataSource> weak_ptr,
      base::WeakPtr<BlobDataSourceCore> weak_core,
      std::unique_ptr<BlobDataSourceCore> core);

  void WaitForCore(base::OnceClosure callback);

  void ReadToDataPipeImpl(uint64_t offset,
                          uint64_t length,
                          mojo::ScopedDataPipeProducerHandle producer_handle,
                          CompletionCallback callback);

  void SetCoreOnUIImpl(base::WeakPtr<BlobDataSourceCore> weak_core,
                       std::unique_ptr<BlobDataSourceCore> core);

  // Used to call BlobDataSourceCore's method on the IO thread.
  base::WeakPtr<BlobDataSourceCore> weak_core_;
  // Owned by |this|. Must be deleted on the IO thread.
  std::unique_ptr<BlobDataSourceCore> core_;
  // Used to wait SetCoreOnUI() to be called.
  std::vector<base::OnceClosure> pending_get_core_tasks_;

  base::WeakPtrFactory<BundledExchangesBlobDataSource> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesBlobDataSource);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_BLOB_DATA_SOURCE_H_
