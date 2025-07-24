// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/url_loading/url_loader.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "components/web_package/web_bundle_utils.h"
#include "components/webapps/isolated_web_apps/reading/response_reader.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_registry.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_registry_factory.h"
#include "components/webapps/isolated_web_apps/url_loading/utils.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/loading_params.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace web_app {

namespace {

class IsolatedWebAppURLLoaderImpl : public network::mojom::URLLoader {
 public:
  IsolatedWebAppURLLoaderImpl(
      const base::FilePath& web_bundle_path,
      bool dev_mode,
      const web_package::SignedWebBundleId& web_bundle_id,
      mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
      const network::ResourceRequest& resource_request,
      std::optional<content::FrameTreeNodeId> frame_tree_node_id)
      : loader_client_(std::move(loader_client)),
        resource_request_(resource_request),
        frame_tree_node_id_(frame_tree_node_id),
        web_bundle_path_(web_bundle_path),
        dev_mode_(dev_mode),
        web_bundle_id_(web_bundle_id) {}

  IsolatedWebAppURLLoaderImpl(const IsolatedWebAppURLLoaderImpl&) = delete;
  IsolatedWebAppURLLoaderImpl& operator=(const IsolatedWebAppURLLoaderImpl&) =
      delete;
  ~IsolatedWebAppURLLoaderImpl() override = default;

  void Start(content::BrowserContext* browser_context) {
    IsolatedWebAppReaderRegistryFactory::Get(browser_context)
        ->ReadResponse(
            web_bundle_path_, dev_mode_, web_bundle_id_, resource_request_,
            base::BindOnce(&IsolatedWebAppURLLoaderImpl::OnResponseRead,
                           weak_factory_.GetWeakPtr()));
  }

 private:
  void OnResponseRead(
      base::expected<IsolatedWebAppResponseReader::Response,
                     IsolatedWebAppReaderRegistry::ReadResponseError>
          response) {
    if (!loader_client_.is_connected()) {
      return;
    }

    if (!response.has_value()) {
      LogErrorMessageToConsole(frame_tree_node_id_, response.error().message);
      switch (response.error().type) {
        case IsolatedWebAppReaderRegistry::ReadResponseError::Type::kOtherError:
          loader_client_->OnComplete(
              network::URLLoaderCompletionStatus(net::ERR_INVALID_WEB_BUNDLE));
          return;
        case IsolatedWebAppReaderRegistry::ReadResponseError::Type::
            kResponseNotFound:
          // Return a synthetic 404 response.
          CompleteWithGeneratedResponse(std::move(loader_client_),
                                        net::HTTP_NOT_FOUND);
          return;
      }
    }

    // TODO(crbug.com/41474458): For the initial implementation, we allow only
    // net::HTTP_OK, but we should clarify acceptable status code in the spec.
    if (response->head()->response_code != net::HTTP_OK) {
      LogErrorMessageToConsole(
          frame_tree_node_id_,
          base::StringPrintf(
              "Failed to read response from Signed Web Bundle: The response "
              "has an unsupported HTTP status code: %d (only status code %d is "
              "allowed).",
              response->head()->response_code, net::HTTP_OK));
      loader_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INVALID_WEB_BUNDLE));
      return;
    }

    std::string header_string =
        web_package::CreateHeaderString(response->head());
    auto response_head =
        web_package::CreateResourceResponseFromHeaderString(header_string);
    response_head->content_length = response->head()->payload_length;
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes =
        std::min(uint64_t{network::GetDataPipeDefaultAllocationSize()},
                 response->head()->payload_length);

    auto result =
        mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
    if (result != MOJO_RESULT_OK) {
      loader_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
      return;
    }
    header_length_ = header_string.size();
    body_length_ = response_head->content_length;
    loader_client_->OnReceiveResponse(std::move(response_head),
                                      std::move(consumer_handle), std::nullopt);

    response->ReadBody(
        std::move(producer_handle),
        base::BindOnce(&IsolatedWebAppURLLoaderImpl::FinishReadingBody,
                       weak_factory_.GetWeakPtr()));
  }

  void FinishReadingBody(net::Error net_error) {
    if (!loader_client_.is_connected()) {
      return;
    }

    network::URLLoaderCompletionStatus status(net_error);
    // For these values we use the same `body_length_` as we don't currently
    // provide encoding in Web Bundles.
    status.encoded_data_length = body_length_ + header_length_;
    status.encoded_body_length = body_length_;
    status.decoded_body_length = body_length_;
    loader_client_->OnComplete(status);
  }

  // network::mojom::URLLoader implementation
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {
    NOTREACHED();
  }
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {}

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_;
  int64_t header_length_;
  int64_t body_length_;
  const network::ResourceRequest resource_request_;
  std::optional<content::FrameTreeNodeId> frame_tree_node_id_;
  const base::FilePath web_bundle_path_;
  const bool dev_mode_;
  const web_package::SignedWebBundleId web_bundle_id_;

  base::WeakPtrFactory<IsolatedWebAppURLLoaderImpl> weak_factory_{this};
};

}  // namespace

// static
void IsolatedWebAppURLLoader::CreateAndStart(
    content::BrowserContext* browser_context,
    const base::FilePath& web_bundle_path,
    bool dev_mode,
    const web_package::SignedWebBundleId& web_bundle_id,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
    const network::ResourceRequest& resource_request,
    const std::optional<content::FrameTreeNodeId>& frame_tree_node_id) {
  // `browser_context` is not stored in the class to prevent having to deal with
  // dangling pointers (as this is a self-owned class without a shutdown
  // notifier).
  auto loader = std::make_unique<IsolatedWebAppURLLoaderImpl>(
      web_bundle_path, dev_mode, web_bundle_id, std::move(loader_client),
      resource_request, frame_tree_node_id);
  auto* raw_loader = loader.get();
  mojo::MakeSelfOwnedReceiver(std::move(loader), std::move(loader_receiver));
  raw_loader->Start(browser_context);
}

}  // namespace web_app
