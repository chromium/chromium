// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_url_loader_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool.h"
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
#include "content/public/browser/non_network_url_loader_factory_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/parsed_headers.h"
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
    scoped_refptr<base::RefCountedMemory> bytes) {
  if (!bytes) {
    CallOnError(std::move(client_remote), net::ERR_FAILED);
    return;
  }

  if (replacements) {
    // We won't know the the final output size ahead of time, so we have to
    // use an intermediate string.
    base::StringPiece input(reinterpret_cast<const char*>(bytes->front()),
                            bytes->size());
    std::string temp_str;
    if (replace_in_js) {
      CHECK(
          ui::ReplaceTemplateExpressionsInJS(input, *replacements, &temp_str));
    } else {
      temp_str = ui::ReplaceTemplateExpressions(input, *replacements);
    }
    bytes = base::RefCountedString::TakeString(&temp_str);
  }

  uint32_t output_size = bytes->size();

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = output_size;
  mojo::ScopedDataPipeProducerHandle pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle pipe_consumer_handle;
  MojoResult create_result = mojo::CreateDataPipe(
      &options, &pipe_producer_handle, &pipe_consumer_handle);
  CHECK_EQ(create_result, MOJO_RESULT_OK);

  void* buffer = nullptr;
  uint32_t num_bytes = output_size;
  MojoResult result = pipe_producer_handle->BeginWriteData(
      &buffer, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  CHECK_EQ(result, MOJO_RESULT_OK);
  CHECK_GE(num_bytes, output_size);

  memcpy(buffer, bytes->front(), output_size);
  result = pipe_producer_handle->EndWriteData(output_size);
  CHECK_EQ(result, MOJO_RESULT_OK);

  // For media content, |content_length| must be known upfront for data that is
  // assumed to be fully buffered (as opposed to streamed from the network),
  // otherwise the media player will get confused and refuse to play.
  // Content delivered via chrome:// URLs is assumed fully buffered.
  headers->content_length = output_size;

  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(client_remote));
  client->OnReceiveResponse(std::move(headers));

  client->OnStartLoadingResponseBody(std::move(pipe_consumer_handle));
  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = output_size;
  status.encoded_body_length = output_size;
  status.decoded_body_length = output_size;
  client->OnComplete(status);
}

void DataAvailable(
    network::mojom::URLResponseHeadPtr headers,
    const ui::TemplateReplacements* replacements,
    bool replace_in_js,
    scoped_refptr<URLDataSourceImpl> source,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    scoped_refptr<base::RefCountedMemory> bytes) {
  // Since the bytes are from the memory mapped resource file, copying the
  // data can lead to disk access. Needs to be posted to a SequencedTaskRunner
  // as Mojo requires a SequencedTaskRunnerHandle in scope.
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
      ->PostTask(FROM_HERE, base::BindOnce(ReadData, std::move(headers),
                                           replacements, replace_in_js, source,
                                           std::move(client_remote), bytes));
}

void StartURLLoader(
    const network::ResourceRequest& request,
    int frame_tree_node_id,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    BrowserContext* browser_context) {
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

  std::string path = URLDataSource::URLToRequestPath(request.url);
  std::string origin_header;
  request.headers.GetHeader(net::HttpRequestHeaders::kOrigin, &origin_header);

  scoped_refptr<net::HttpResponseHeaders> headers =
      URLDataManagerBackend::GetHeaders(source, path, origin_header);

  auto resource_response = network::mojom::URLResponseHead::New();

  resource_response->headers = headers;
  // Headers from WebUI are trusted, so parsing can happen from a non-sandboxed
  // process.
  resource_response->parsed_headers =
      network::PopulateParsedHeaders(resource_response->headers, request.url);
  resource_response->mime_type = source->source()->GetMimeType(path);
  // TODO: fill all the time related field i.e. request_time response_time
  // request_start response_start

  WebContents::Getter wc_getter =
      base::BindRepeating(WebContents::FromFrameTreeNodeId, frame_tree_node_id);

  bool replace_in_js =
      source->source()->ShouldReplaceI18nInJS() &&
      source->source()->GetMimeType(path) == "application/javascript";

  const ui::TemplateReplacements* replacements = nullptr;
  if (source->source()->GetMimeType(path) == "text/html" || replace_in_js)
    replacements = source->source()->GetReplacements();

  // To keep the same behavior as the old WebUI code, we call the source to get
  // the value for |replacements| on the IO thread. Since |replacements| is
  // owned by |source| keep a reference to it in the callback.
  URLDataSource::GotDataCallback data_available_callback = base::BindOnce(
      DataAvailable, std::move(resource_response), replacements, replace_in_js,
      base::RetainedRef(source), std::move(client_remote));

  source->source()->StartDataRequest(request.url, std::move(wc_getter),
                                     std::move(data_available_callback));
}

class WebUIURLLoaderFactory : public NonNetworkURLLoaderFactoryBase {
 public:
  // Returns mojo::PendingRemote to a newly constructed WebUIURLLoaderFactory.
  // The factory is self-owned - it will delete itself once there are no more
  // receivers (including the receiver associated with the returned
  // mojo::PendingRemote and the receivers bound by the Clone method).
  //
  // |allowed_hosts| is an optional set of allowed host names. If empty then
  // all hosts are allowed.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create(
      FrameTreeNode* ftn,
      const std::string& scheme,
      base::flat_set<std::string> allowed_hosts) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

    // The WebUIURLLoaderFactory will delete itself when there are no more
    // receivers - see the NonNetworkURLLoaderFactoryBase::OnDisconnect method.
    new WebUIURLLoaderFactory(ftn, scheme, std::move(allowed_hosts),
                              pending_remote.InitWithNewPipeAndPassReceiver());

    return pending_remote;
  }

 private:
  ~WebUIURLLoaderFactory() override = default;

  // network::mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    auto* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
    if (!ftn) {
      CallOnError(std::move(client), net::ERR_FAILED);
      return;
    }

    BrowserContext* browser_context =
        ftn->current_frame_host()->GetBrowserContext();

    if (request.url.scheme() != scheme_) {
      DVLOG(1) << "Bad scheme: " << request.url.scheme();
      mojo::ReportBadMessage("Incorrect scheme");
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    if (!allowed_hosts_.empty() &&
        (!request.url.has_host() ||
         allowed_hosts_.find(request.url.host()) == allowed_hosts_.end())) {
      // Temporary reporting the bad WebUI host for for http://crbug.com/837328.
      static auto* crash_key = base::debug::AllocateCrashKeyString(
          "webui_url", base::debug::CrashKeySize::Size64);
      base::debug::SetCrashKeyString(crash_key, request.url.spec());

      DVLOG(1) << "Bad host: \"" << request.url.host() << '"';
      mojo::ReportBadMessage("Incorrect host");
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    if (request.url.host_piece() == kChromeUIBlobInternalsHost) {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              &StartBlobInternalsURLLoader, request, std::move(client),
              base::Unretained(
                  ChromeBlobStorageContext::GetFor(browser_context))));
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
                   browser_context);
  }

  const std::string& scheme() const { return scheme_; }

  WebUIURLLoaderFactory(
      FrameTreeNode* ftn,
      const std::string& scheme,
      base::flat_set<std::string> allowed_hosts,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
      : NonNetworkURLLoaderFactoryBase(std::move(factory_receiver)),
        frame_tree_node_id_(ftn->frame_tree_node_id()),
        scheme_(scheme),
        allowed_hosts_(std::move(allowed_hosts)) {}

  int const frame_tree_node_id_;
  const std::string scheme_;
  const base::flat_set<std::string> allowed_hosts_;  // if empty all allowed.

  DISALLOW_COPY_AND_ASSIGN(WebUIURLLoaderFactory);
};

}  // namespace

mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateWebUIURLLoaderFactory(RenderFrameHost* render_frame_host,
                            const std::string& scheme,
                            base::flat_set<std::string> allowed_hosts) {
  return WebUIURLLoaderFactory::Create(FrameTreeNode::From(render_frame_host),
                                       scheme, std::move(allowed_hosts));
}

}  // namespace content
