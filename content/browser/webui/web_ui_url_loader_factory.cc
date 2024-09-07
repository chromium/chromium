// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_url_loader_factory.h"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/blob_internals_url_loader.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webui/network_error_url_loader.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "ui/base/template_expressions.h"

namespace content {

namespace {

class WebUIURLLoaderFactory;

void CallOnError(
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    int error_code) {
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(client_remote));

  network::URLLoaderCompletionStatus status;
  status.error_code = error_code;
  client->OnComplete(status);
}

void ReadData(
    network::mojom::URLResponseHeadPtr headers,
    const ui::TemplateReplacements* replacements,
    bool replace_in_js,
    scoped_refptr<URLDataSourceImpl> data_source,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    std::optional<net::HttpByteRange> requested_range,
    base::ElapsedTimer url_request_elapsed_timer,
    scoped_refptr<base::RefCountedMemory> bytes) {
  TRACE_EVENT0("ui", "WebUIURLLoader::ReadData");
  if (!bytes) {
    CallOnError(std::move(client_remote), net::ERR_FAILED);
    return;
  }

  if (replacements && !replacements->empty()) {
    // We won't know the the final output size ahead of time, so we have to
    // use an intermediate string.
    auto input = base::as_string_view(*bytes);
    std::string temp_str;
    if (replace_in_js) {
      CHECK(
          ui::ReplaceTemplateExpressionsInJS(input, *replacements, &temp_str));
    } else {
      temp_str = ui::ReplaceTemplateExpressions(input, *replacements);
    }
    bytes = base::MakeRefCounted<base::RefCountedString>(std::move(temp_str));
  }

  // The use of MojoCreateDataPipeOptions below means we'll be using uint32_t
  // for sizes / offsets.
  if (!base::IsValueInRangeForNumericType<uint32_t>(bytes->size())) {
    CallOnError(std::move(client_remote), net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }

  uint32_t output_offset = 0;
  size_t output_size = bytes->size();
  if (requested_range) {
    if (!requested_range->ComputeBounds(output_size)) {
      CallOnError(std::move(client_remote),
                  net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
      return;
    }
    DCHECK(base::IsValueInRangeForNumericType<uint32_t>(
        requested_range->first_byte_position()))
        << "Expecting ComputeBounds() to enforce it";
    output_offset = requested_range->first_byte_position();
    output_size = requested_range->last_byte_position() -
                  requested_range->first_byte_position() + 1;
  }

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = output_size;
  mojo::ScopedDataPipeProducerHandle pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle pipe_consumer_handle;
  MojoResult create_result = mojo::CreateDataPipe(
      &options, pipe_producer_handle, pipe_consumer_handle);
  CHECK_EQ(create_result, MOJO_RESULT_OK);

  base::span<uint8_t> buffer;
  MojoResult result = pipe_producer_handle->BeginWriteData(
      output_size, MOJO_WRITE_DATA_FLAG_NONE, buffer);
  CHECK_EQ(result, MOJO_RESULT_OK);
  CHECK_GE(buffer.size(), output_size);
  CHECK_LE(output_offset + output_size, bytes->size());

  buffer.copy_prefix_from(
      base::span(*bytes).subspan(output_offset, output_size));
  result = pipe_producer_handle->EndWriteData(output_size);
  CHECK_EQ(result, MOJO_RESULT_OK);

  // For media content, |content_length| must be known upfront for data that is
  // assumed to be fully buffered (as opposed to streamed from the network),
  // otherwise the media player will get confused and refuse to play.
  // Content delivered via chrome:// URLs is assumed fully buffered.
  headers->content_length = output_size;

  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(client_remote));

  client->OnReceiveResponse(std::move(headers), std::move(pipe_consumer_handle),
                            std::nullopt);

  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = output_size;
  status.encoded_body_length = output_size;
  status.decoded_body_length = output_size;
  client->OnComplete(status);

  UMA_HISTOGRAM_TIMES("WebUI.WebUIURLLoaderFactory.URLRequestLoadTime",
                      url_request_elapsed_timer.Elapsed());
}

void DataAvailable(
    network::mojom::URLResponseHeadPtr headers,
    const ui::TemplateReplacements* replacements,
    bool replace_in_js,
    scoped_refptr<URLDataSourceImpl> source,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    std::optional<net::HttpByteRange> requested_range,
    base::ElapsedTimer url_request_elapsed_timer,
    scoped_refptr<base::RefCountedMemory> bytes) {
  TRACE_EVENT0("ui", "WebUIURLLoader::DataAvailable");
  // Since the bytes are from the memory mapped resource file, copying the
  // data can lead to disk access. Needs to be posted to a SequencedTaskRunner
  // as Mojo requires a SequencedTaskRunner::CurrentDefaultHandle in scope.
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
      ->PostTask(FROM_HERE,
                 base::BindOnce(ReadData, std::move(headers), replacements,
                                replace_in_js, source, std::move(client_remote),
                                std::move(requested_range),
                                std::move(url_request_elapsed_timer), bytes));
}

void StartURLLoader(
    const network::ResourceRequest& request,
    FrameTreeNodeId frame_tree_node_id,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    BrowserContext* browser_context) {
  base::ElapsedTimer url_request_elapsed_timer;

  // NOTE: this duplicates code in URLDataManagerBackend::StartRequest.
  if (!URLDataManagerBackend::CheckURLIsValid(request.url)) {
    CallOnError(std::move(client_remote), net::ERR_INVALID_URL);
    return;
  }

  URLDataSourceImpl* source =
      URLDataManagerBackend::GetForBrowserContext(browser_context)
          ->GetDataSourceFromURL(request.url);
  if (!source) {
    CallOnError(std::move(client_remote), net::ERR_INVALID_URL);
    return;
  }

  if (!source->source()->ShouldServiceRequest(request.url, browser_context,
                                              -1)) {
    CallOnError(std::move(client_remote), net::ERR_INVALID_URL);
    return;
  }

  // Load everything by default, but respect the Range header if present.
  std::optional<net::HttpByteRange> range;
  if (std::optional<std::string> range_header =
          request.headers.GetHeader(net::HttpRequestHeaders::kRange);
      range_header) {
    std::vector<net::HttpByteRange> ranges;
    // For simplicity, only allow a single range. This is expected to be
    // sufficient for WebUI content.
    if (!net::HttpUtil::ParseRangeHeader(*range_header, &ranges) ||
        ranges.size() > 1u || !ranges[0].IsValid()) {
      CallOnError(std::move(client_remote),
                  net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
      return;
    }
    range = ranges[0];
  }

  std::string path = URLDataSource::URLToRequestPath(request.url);
  std::string origin_header =
      request.headers.GetHeader(net::HttpRequestHeaders::kOrigin)
          .value_or(std::string());

  scoped_refptr<net::HttpResponseHeaders> headers =
      URLDataManagerBackend::GetHeaders(source, request.url, origin_header);

  auto resource_response = network::mojom::URLResponseHead::New();

  resource_response->headers = headers;
  // Headers from WebUI are trusted, so parsing can happen from a non-sandboxed
  // process.
  resource_response->parsed_headers = network::PopulateParsedHeaders(
      resource_response->headers.get(), request.url);
  resource_response->mime_type = source->source()->GetMimeType(request.url);
  // TODO: fill all the time related field i.e. request_time response_time
  // request_start response_start

  WebContents::Getter wc_getter;

  // Service Workers factories have no associated frame.
  if (frame_tree_node_id.is_null()) {
    wc_getter = base::BindRepeating([]() -> WebContents* { return nullptr; });
  } else {
    wc_getter = base::BindRepeating(WebContents::FromFrameTreeNodeId,
                                    frame_tree_node_id);
  }

  bool replace_in_js =
      source->source()->ShouldReplaceI18nInJS() &&
      source->source()->GetMimeType(request.url) == "application/javascript";

  const ui::TemplateReplacements* replacements = nullptr;
  const std::string mime_type = source->source()->GetMimeType(request.url);
  if (mime_type == "text/html" || mime_type == "text/css" || replace_in_js)
    replacements = source->source()->GetReplacements();

  // To keep the same behavior as the old WebUI code, we call the source to get
  // the value for |replacements| on the IO thread. Since |replacements| is
  // owned by |source| keep a reference to it in the callback.
  URLDataSource::GotDataCallback data_available_callback = base::BindOnce(
      DataAvailable, std::move(resource_response), replacements, replace_in_js,
      base::RetainedRef(source), std::move(client_remote), std::move(range),
      std::move(url_request_elapsed_timer));

  source->source()->StartDataRequest(request.url, std::move(wc_getter),
                                     std::move(data_available_callback));
}

class WebUIURLLoaderFactory : public network::SelfDeletingURLLoaderFactory {
 public:
  // Returns mojo::PendingRemote to a newly constructed WebUIURLLoaderFactory.
  // The factory is self-owned - it will delete itself once there are no more
  // receivers (including the receiver associated with the returned
  // mojo::PendingRemote and the receivers bound by the Clone method).
  //
  // |allowed_hosts| is an optional set of allowed host names. If empty then
  // all hosts are allowed.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> CreateForFrame(
      FrameTreeNode* ftn,
      const std::string& scheme,
      base::flat_set<std::string> allowed_hosts) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

    // The WebUIURLLoaderFactory will delete itself when there are no more
    // receivers - see the
    // network::SelfDeletingURLLoaderFactory::OnDisconnect method.
    new WebUIURLLoaderFactory(ftn->current_frame_host()->GetBrowserContext(),
                              ftn->frame_tree_node_id(), scheme,
                              std::move(allowed_hosts),
                              pending_remote.InitWithNewPipeAndPassReceiver());
    return pending_remote;
  }

  static mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateForServiceWorker(BrowserContext* browser_context,
                         const std::string& scheme,
                         base::flat_set<std::string> allowed_hosts) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

    // The WebUIURLLoaderFactory will delete itself when there are no more
    // receivers - see the
    // network::SelfDeletingURLLoaderFactory::OnDisconnect method.
    new WebUIURLLoaderFactory(browser_context, FrameTreeNodeId(), scheme,
                              std::move(allowed_hosts),
                              pending_remote.InitWithNewPipeAndPassReceiver());
    return pending_remote;
  }

  WebUIURLLoaderFactory(const WebUIURLLoaderFactory&) = delete;
  WebUIURLLoaderFactory& operator=(const WebUIURLLoaderFactory&) = delete;

 private:
  ~WebUIURLLoaderFactory() override = default;

  // network::mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (browser_context_.WasInvalidated()) {
      DVLOG(1) << "Context has been destroyed";
      CallOnError(std::move(client), net::ERR_FAILED);
      DisconnectReceiversAndDestroy();
      return;
    }

    if (frame_tree_node_id_ &&
        !FrameTreeNode::GloballyFindByID(frame_tree_node_id_)) {
      CallOnError(std::move(client), net::ERR_FAILED);
      return;
    }

    if (request.url.scheme() != scheme_) {
      DVLOG(1) << "Bad scheme: " << request.url.scheme();
      SCOPED_CRASH_KEY_STRING32("WebUI", "actual_scheme", request.url.scheme());
      SCOPED_CRASH_KEY_STRING32("WebUI", "expected_scheme", scheme_);
      SCOPED_CRASH_KEY_STRING64("WebUI", "requested_url", request.url.spec());
      mojo::ReportBadMessage("Incorrect scheme");
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    CHECK(allowed_hosts_.empty() ||
          (request.url.has_host() &&
           allowed_hosts_.find(request.url.host()) != allowed_hosts_.end()))
        << "Incorrect host: " << request.url.host();

    if (request.url.host_piece() == kChromeUIBlobInternalsHost) {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              &StartBlobInternalsURLLoader, request, std::move(client),
              base::Unretained(
                  ChromeBlobStorageContext::GetFor(browser_context_.get()))));
      return;
    }

    if (request.url.host_piece() == kChromeUINetworkErrorHost ||
        request.url.host_piece() == kChromeUIDinoHost) {
      StartNetworkErrorsURLLoader(request, std::move(client));
      return;
    }

    // We pass the FrameTreeNode ID to get to the WebContents because requests
    // from frames can happen while the RFH is changed for a cross-process
    // navigation. The URLDataSources just need the WebContents; the specific
    // frame doesn't matter.
    StartURLLoader(request, frame_tree_node_id_, std::move(client),
                   browser_context_.get());
  }

  const std::string& scheme() const { return scheme_; }

  WebUIURLLoaderFactory(
      BrowserContext* browser_context,
      FrameTreeNodeId frame_tree_node_id,
      const std::string& scheme,
      base::flat_set<std::string> allowed_hosts,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
      : network::SelfDeletingURLLoaderFactory(std::move(factory_receiver)),
        browser_context_(browser_context->GetWeakPtr()),
        frame_tree_node_id_(frame_tree_node_id),
        scheme_(scheme),
        allowed_hosts_(std::move(allowed_hosts)) {}

  base::WeakPtr<BrowserContext> browser_context_;
  const FrameTreeNodeId frame_tree_node_id_;
  const std::string scheme_;
  const base::flat_set<std::string> allowed_hosts_;  // if empty all allowed.
};

}  // namespace

mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateWebUIURLLoaderFactory(RenderFrameHost* render_frame_host,
                            const std::string& scheme,
                            base::flat_set<std::string> allowed_hosts) {
  return WebUIURLLoaderFactory::CreateForFrame(
      FrameTreeNode::From(render_frame_host), scheme, std::move(allowed_hosts));
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateWebUIServiceWorkerLoaderFactory(
    BrowserContext* browser_context,
    const std::string& scheme,
    base::flat_set<std::string> allowed_hosts) {
  return WebUIURLLoaderFactory::CreateForServiceWorker(
      browser_context, scheme, std::move(allowed_hosts));
}

}  // namespace content
