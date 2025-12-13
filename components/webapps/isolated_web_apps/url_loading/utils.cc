// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/url_loading/utils.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace web_app {

namespace {

GURL ConstructProxyUrl(const IwaSourceProxy& proxy,
                       const GURL& resource_request_url) {
  GURL::Replacements replacements;
  replacements.SetPathStr(resource_request_url.path());
  if (resource_request_url.has_query()) {
    replacements.SetQueryStr(resource_request_url.query());
  }
  return proxy.proxy_url().GetURL().ReplaceComponents(replacements);
}

network::ResourceRequest ConstructProxyRequest(
    const IwaSourceProxy& proxy,
    const network::ResourceRequest& resource_request) {
  // Create a new ResourceRequest with the proxy URL.
  network::ResourceRequest proxy_request;
  proxy_request.url = ConstructProxyUrl(proxy, resource_request.url);
  proxy_request.method = net::HttpRequestHeaders::kGetMethod;
  // Don't send cookies or HTTP authentication to the proxy server.
  proxy_request.credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::string accept_header_value =
      resource_request.headers.GetHeader(net::HttpRequestHeaders::kAccept)
          .value_or(network::kDefaultAcceptHeaderValue);
  proxy_request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                  accept_header_value);
  proxy_request.headers.SetHeader(net::HttpRequestHeaders::kCacheControl,
                                  "no-cache");
  return proxy_request;
}

content::WebContents* FromFrameTreeNodeID(
    const std::optional<content::FrameTreeNodeId>& frame_tree_node_id) {
  if (!frame_tree_node_id) {
    return nullptr;
  }
  return content::WebContents::FromFrameTreeNodeId(*frame_tree_node_id);
}

}  // namespace

void CompleteWithGeneratedResponse(
    mojo::Remote<network::mojom::URLLoaderClient> loader_client,
    net::HttpStatusCode http_status_code,
    std::optional<std::string> body,
    std::string_view content_type) {
  size_t content_length = body.has_value() ? body->size() : 0;
  std::string headers = base::StringPrintf(
      "HTTP/1.1 %d %s\n"
      "Content-Type: %s;charset=utf-8\n"
      "Content-Length: %s\n\n",
      static_cast<int>(http_status_code),
      net::GetHttpReasonPhrase(http_status_code), content_type.data(),
      base::NumberToString(content_length).c_str());
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  response_head->headers->GetMimeTypeAndCharset(&response_head->mime_type,
                                                &response_head->charset);
  response_head->content_length = content_length;

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  mojo::ScopedDataPipeProducerHandle producer_handle;

  auto result = mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle);
  if (result != MOJO_RESULT_OK) {
    loader_client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }

  loader_client->OnReceiveResponse(std::move(response_head),
                                   std::move(consumer_handle),
                                   /*cached_metadata=*/std::nullopt);

  if (body.has_value()) {
    size_t actually_written_bytes = 0;
    MojoResult write_result = producer_handle->WriteData(
        base::as_byte_span(*body), MOJO_WRITE_DATA_FLAG_NONE,
        actually_written_bytes);
    if (write_result != MOJO_RESULT_OK ||
        actually_written_bytes != body->size()) {
      loader_client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }
  } else {
    producer_handle.reset();
  }

  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = headers.size() + content_length;
  status.encoded_body_length = content_length;
  status.decoded_body_length = content_length;
  loader_client->OnComplete(status);
}

void LogErrorMessageToConsole(
    std::optional<content::FrameTreeNodeId> frame_tree_node_id,
    const std::string& error_message) {
  content::WebContents* web_contents = FromFrameTreeNodeID(frame_tree_node_id);
  if (!web_contents) {
    // Log to the terminal if we can't log to the console.
    LOG(ERROR) << error_message;
    return;
  }
  // TODO(crbug.com/40239529): The console message will vanish from the
  // console if the user does not have the `Preserve Log` option enabled,
  // since it is triggered before the navigation commits. We should try to use
  // a similar approach as in crrev.com/c/3397976, but `FrameTreeNode` is not
  // part of content/public.

  // Find the `RenderFrameHost` associated with the `FrameTreeNode`
  // corresponding to the `frame_tree_node_id`, and then log the message.
  web_contents->ForEachRenderFrameHostWithAction(
      [frame_tree_node_id,
       &error_message](content::RenderFrameHost* render_frame_host) {
        if (render_frame_host->GetFrameTreeNodeId() == frame_tree_node_id) {
          render_frame_host->AddMessageToConsole(
              blink::mojom::ConsoleMessageLevel::kError, error_message);
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
}

void LogErrorAndFail(
    const std::string& error_message,
    const std::optional<content::FrameTreeNodeId>& frame_tree_node_id,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  LogErrorMessageToConsole(frame_tree_node_id, error_message);

  mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
      ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
}

void HandleProxy(
    content::BrowserContext* browser_context,
    const web_package::SignedWebBundleId& web_bundle_id,
    const IwaSourceProxy& proxy,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
    const network::ResourceRequest& resource_request,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    const std::optional<content::FrameTreeNodeId>& frame_tree_node_id) {
  DCHECK(!proxy.proxy_url().opaque());
  content::StoragePartition* storage_partition =
      browser_context->GetStoragePartition(
          IwaOrigin(web_bundle_id).storage_partition_config(browser_context),
          /*can_create=*/false);
  if (!storage_partition) {
    LogErrorAndFail("Storage not found for Isolated Web App: " +
                        resource_request.url.spec(),
                    frame_tree_node_id, std::move(loader_client));
    return;
  }

  storage_partition->GetURLLoaderFactoryForBrowserProcess()
      ->CreateLoaderAndStart(std::move(loader_receiver),
                             /*request_id=*/0,
                             network::mojom::kURLLoadOptionNone,
                             ConstructProxyRequest(proxy, resource_request),
                             std::move(loader_client), traffic_annotation);
}

}  // namespace web_app
