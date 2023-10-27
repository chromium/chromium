// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_localization_throttle.h"

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/shared_l10n_map.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/platform/web_url.h"
#include "url/gurl.h"

namespace extensions {

namespace {

const char kCancelReason[] = "ExtensionLocalizationThrottle";

class ExtensionLocalizationURLLoader : public network::mojom::URLLoaderClient,
                                       public network::mojom::URLLoader,
                                       public mojo::DataPipeDrainer::Client {
 public:
  ExtensionLocalizationURLLoader(
      int render_frame_id,
      const std::string& extension_id,
      mojo::PendingRemote<network::mojom::URLLoaderClient>
          destination_url_loader_client)
      : render_frame_id_(render_frame_id),
        extension_id_(extension_id),
        destination_url_loader_client_(
            std::move(destination_url_loader_client)) {}
  ~ExtensionLocalizationURLLoader() override = default;

  bool Start(
      mojo::PendingRemote<network::mojom::URLLoader> source_url_loader_remote,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>
          source_url_client_receiver,
      mojo::ScopedDataPipeConsumerHandle body,
      mojo::ScopedDataPipeProducerHandle producer_handle) {
    source_url_loader_.Bind(std::move(source_url_loader_remote));
    source_url_client_receiver_.Bind(std::move(source_url_client_receiver));

    data_drainer_ =
        std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));
    producer_handle_ = std::move(producer_handle);
    return true;
  }

  // network::mojom::URLLoaderClient implementation (called from the source of
  // the response):
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
    // OnReceiveEarlyHints() shouldn't be called because
    // ExtensionLocalizationURLLoader is
    // created by ExtensionLocalizationThrottle::WillProcessResponse(), which is
    // equivalent to OnReceiveResponse().
    NOTREACHED();
  }
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override {
    // OnReceiveResponse() shouldn't be called because
    // ExtensionLocalizationURLLoader is
    // created by ExtensionLocalizationThrottle::WillProcessResponse(), which is
    // equivalent to OnReceiveResponse().
    NOTREACHED();
  }
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {
    // OnReceiveRedirect() shouldn't be called because
    // ExtensionLocalizationURLLoader is
    // created by ExtensionLocalizationThrottle::WillProcessResponse(), which is
    // equivalent to OnReceiveResponse().
    NOTREACHED();
  }
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    // OnUploadProgress() shouldn't be called because
    // ExtensionLocalizationURLLoader is
    // created by ExtensionLocalizationThrottle::WillProcessResponse(), which is
    // equivalent to OnReceiveResponse().
    NOTREACHED();
  }
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    destination_url_loader_client_->OnTransferSizeUpdated(transfer_size_diff);
  }
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    original_complete_status_ = status;
    MaybeSendOnComplete();
  }

  // network::mojom::URLLoader implementation (called from the destination of
  // the response):
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override {
    // ExtensionLocalizationURLLoader starts handling the request after
    // OnReceivedResponse(). A redirect response is not expected.
    NOTREACHED();
  }
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    source_url_loader_->SetPriority(priority, intra_priority_value);
  }
  void PauseReadingBodyFromNet() override {
    source_url_loader_->PauseReadingBodyFromNet();
  }
  void ResumeReadingBodyFromNet() override {
    source_url_loader_->ResumeReadingBodyFromNet();
  }

  // mojo::DataPipeDrainer
  void OnDataAvailable(const void* data, size_t num_bytes) override {
    data_.append(static_cast<const char*>(data), num_bytes);
  }
  void OnDataComplete() override {
    data_drainer_.reset();

    if (!data_.empty()) {
      ReplaceMessages();
    }

    auto data_producer =
        std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle_));
    auto data = std::make_unique<std::string>(std::move(data_));
    // To avoid unnecessary string copy, use STRING_STAYS_VALID_UNTIL_COMPLETION
    // here, and keep the original data hold in the closure below.
    auto source = std::make_unique<mojo::StringDataSource>(
        *data, mojo::StringDataSource::AsyncWritingMode::
                   STRING_STAYS_VALID_UNTIL_COMPLETION);
    mojo::DataPipeProducer* data_producer_ptr = data_producer.get();
    data_producer_ptr->Write(
        std::move(source),
        base::BindOnce(
            [](std::unique_ptr<mojo::DataPipeProducer> data_producer,
               std::unique_ptr<std::string> data,
               base::OnceCallback<void(MojoResult)> on_data_written_callback,
               MojoResult result) {
              std::move(on_data_written_callback).Run(result);
            },
            std::move(data_producer), std::move(data),
            base::BindOnce(&ExtensionLocalizationURLLoader::OnDataWritten,
                           weak_factory_.GetWeakPtr())));
  }

 private:
  void OnDataWritten(MojoResult result) {
    data_write_result_ = result;
    MaybeSendOnComplete();
  }

  void MaybeSendOnComplete() {
    if (!original_complete_status_ || !data_write_result_) {
      return;
    }
    if (*data_write_result_ != MOJO_RESULT_OK) {
      destination_url_loader_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    } else {
      destination_url_loader_client_->OnComplete(*original_complete_status_);
    }
  }

  void ReplaceMessages() {
    extensions::SharedL10nMap::IPCTarget* ipc_target = nullptr;
#if BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
    ipc_target = content::RenderThread::Get();
    (void)render_frame_id_;
#else
    content::RenderFrame* render_frame =
        content::RenderFrame::FromRoutingID(render_frame_id_);
    if (render_frame) {
      ipc_target = ExtensionFrameHelper::Get(render_frame)->GetRendererHost();
    }
#endif
    extensions::SharedL10nMap::GetInstance().ReplaceMessages(
        extension_id_, &data_, ipc_target);
  }

  const int render_frame_id_;
  const ExtensionId extension_id_;
  std::unique_ptr<mojo::DataPipeDrainer> data_drainer_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  std::string data_;

  absl::optional<network::URLLoaderCompletionStatus> original_complete_status_;
  absl::optional<MojoResult> data_write_result_;

  mojo::Receiver<network::mojom::URLLoaderClient> source_url_client_receiver_{
      this};
  mojo::Remote<network::mojom::URLLoader> source_url_loader_;
  mojo::Remote<network::mojom::URLLoaderClient> destination_url_loader_client_;
  base::WeakPtrFactory<ExtensionLocalizationURLLoader> weak_factory_{this};
};

}  // namespace

// static
std::unique_ptr<ExtensionLocalizationThrottle>
ExtensionLocalizationThrottle::MaybeCreate(int render_frame_id,
                                           const blink::WebURL& request_url) {
  if (!request_url.ProtocolIs(extensions::kExtensionScheme)) {
    return nullptr;
  }
  return base::WrapUnique(new ExtensionLocalizationThrottle(render_frame_id));
}

ExtensionLocalizationThrottle::ExtensionLocalizationThrottle(
    int render_frame_id)
    : render_frame_id_(render_frame_id) {}

ExtensionLocalizationThrottle::~ExtensionLocalizationThrottle() = default;

void ExtensionLocalizationThrottle::DetachFromCurrentSequence() {}

void ExtensionLocalizationThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  // ExtensionURLLoader can only redirect requests within the
  // chrome-extension:// scheme.
  DCHECK(response_url.SchemeIs(extensions::kExtensionScheme));
  if (!base::StartsWith(response_head->mime_type, "text/css",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return;
  }

  mojo::ScopedDataPipeConsumerHandle body;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoResult create_pipe_result =
      mojo::CreateDataPipe(/*options=*/nullptr, producer_handle, body);

  if (create_pipe_result != MOJO_RESULT_OK || force_error_for_test_) {
    // Synchronous call of `delegate_->CancelWithError` can cause a UAF error.
    // So defer the request here.
    *defer = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(base::BindOnce(
            &ExtensionLocalizationThrottle::DeferredCancelWithError,
            weak_factory_.GetWeakPtr(), net::ERR_INSUFFICIENT_RESOURCES)));
    return;
  }

  mojo::PendingRemote<network::mojom::URLLoader> new_remote;
  mojo::PendingRemote<network::mojom::URLLoaderClient> url_loader_client;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> new_receiver =
      url_loader_client.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<network::mojom::URLLoader> source_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> source_client_receiver;

  auto loader = std::make_unique<ExtensionLocalizationURLLoader>(
      render_frame_id_, response_url.host(), std::move(url_loader_client));

  ExtensionLocalizationURLLoader* loader_rawptr = loader.get();
  // `loader` will be deleted when `new_remote` is disconnected.
  // `new_remote` is binded to ThrottlingURLLoader::url_loader_. So when
  // ThrottlingURLLoader is deleted, `loader` will be deleted.
  mojo::MakeSelfOwnedReceiver(std::move(loader),
                              new_remote.InitWithNewPipeAndPassReceiver());
  delegate_->InterceptResponse(std::move(new_remote), std::move(new_receiver),
                               &source_loader, &source_client_receiver, &body);

  // ExtensionURLLoader always send a valid DataPipeConsumerHandle. So
  // InterceptResponse() must return a valid `body`.
  DCHECK(body);
  loader_rawptr->Start(std::move(source_loader),
                       std::move(source_client_receiver), std::move(body),
                       std::move(producer_handle));
}

void ExtensionLocalizationThrottle::DeferredCancelWithError(int error_code) {
  if (delegate_) {
    delegate_->CancelWithError(error_code, kCancelReason);
  }
}

}  // namespace extensions
