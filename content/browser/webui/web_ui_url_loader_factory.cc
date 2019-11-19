// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_url_loader_factory.h"

#include <map>

#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/blob_internals_url_loader.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/webui/network_error_url_loader.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "ui/base/template_expressions.h"

namespace content {

namespace {

class WebUIURLLoaderFactory;
base::LazyInstance<std::map<GlobalFrameRoutingId,
                            std::unique_ptr<WebUIURLLoaderFactory>>>::Leaky
    g_web_ui_url_loader_factories = LAZY_INSTANCE_INITIALIZER;

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
    scoped_refptr<network::ResourceResponse> headers,
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
  headers->head.content_length = output_size;

  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(client_remote));
  client->OnReceiveResponse(headers->head);

  client->OnStartLoadingResponseBody(std::move(pipe_consumer_handle));
  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = output_size;
  status.encoded_body_length = output_size;
  status.decoded_body_length = output_size;
  client->OnComplete(status);
}

void DataAvailable(
    scoped_refptr<network::ResourceResponse> headers,
    const ui::TemplateReplacements* replacements,
    bool replace_in_js,
    scoped_refptr<URLDataSourceImpl> source,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    scoped_refptr<base::RefCountedMemory> bytes) {
  // Since the bytes are from the memory mapped resource file, copying the
  // data can lead to disk access. Needs to be posted to a SequencedTaskRunner
  // as Mojo requires a SequencedTaskRunnerHandle in scope.
  base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::TaskPriority::USER_BLOCKING, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
      ->PostTask(FROM_HERE,
                 base::BindOnce(ReadData, headers, replacements, replace_in_js,
                                source, std::move(client_remote), bytes));
}

void StartURLLoader(
    const network::ResourceRequest& request,
    int frame_tree_node_id,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    ResourceContext* resource_context) {
  // NOTE: this duplicates code in URLDataManagerBackend::StartRequest.
  if (!URLDataManagerBackend::CheckURLIsValid(request.url)) {
    CallOnError(std::move(client_remote), net::ERR_INVALID_URL);
    return;
  }

  URLDataSourceImpl* source =
      GetURLDataManagerForResourceContext(resource_context)
          ->GetDataSourceFromURL(request.url);
  if (!source) {
    CallOnError(std::move(client_remote), net::ERR_INVALID_URL);
    return;
  }

  if (!source->source()->ShouldServiceRequest(request.url, resource_context,
                                              -1)) {
    CallOnError(std::move(client_remote), net::ERR_INVALID_URL);
    return;
  }

  std::string path = URLDataSource::URLToRequestPath(request.url);
  std::string origin_header;
  request.headers.GetHeader(net::HttpRequestHeaders::kOrigin, &origin_header);

  scoped_refptr<net::HttpResponseHeaders> headers =
      URLDataManagerBackend::GetHeaders(source, path, origin_header);

  scoped_refptr<network::ResourceResponse> resource_response(
      new network::ResourceResponse);
  resource_response->head.headers = headers;
  resource_response->head.mime_type = source->source()->GetMimeType(path);
  // TODO: fill all the time related field i.e. request_time response_time
  // request_start response_start

  WebContents::Getter wc_getter =
      base::Bind(WebContents::FromFrameTreeNodeId, frame_tree_node_id);

  bool replace_in_js =
      source->source()->ShouldReplaceI18nInJS() &&
      source->source()->GetMimeType(path) == "application/javascript";

  const ui::TemplateReplacements* replacements = nullptr;
  if (source->source()->GetMimeType(path) == "text/html" || replace_in_js)
    replacements = source->GetReplacements();

  // To keep the same behavior as the old WebUI code, we call the source to get
  // the value for |replacements| on the IO thread. Since |replacements| is
  // owned by |source| keep a reference to it in the callback.
  auto data_available_callback =
      base::Bind(DataAvailable, resource_response, replacements, replace_in_js,
                 base::RetainedRef(source), base::Passed(&client_remote));

  // TODO(jam): once we only have this code path for WebUI, and not the
  // URLLRequestJob one, then we should switch data sources to run on the UI
  // thread by default.
  scoped_refptr<base::SingleThreadTaskRunner> target_runner =
      source->source()->TaskRunnerForRequestPath(path);
  if (!target_runner) {
    source->source()->StartDataRequest(request.url, std::move(wc_getter),
                                       std::move(data_available_callback));
    return;
  }

  // The DataSource wants StartDataRequest to be called on a specific
  // thread, usually the UI thread, for this path.
  target_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&URLDataSource::StartDataRequest,
                     base::Unretained(source->source()), request.url,
                     std::move(wc_getter), std::move(data_available_callback)));
}

class WebUIURLLoaderFactory : public network::mojom::URLLoaderFactory,
                              public WebContentsObserver {
 public:
  // |allowed_hosts| is an optional set of allowed host names. If empty then
  // all hosts are allowed.
  WebUIURLLoaderFactory(RenderFrameHost* rfh,
                        const std::string& scheme,
                        base::flat_set<std::string> allowed_hosts)
      : WebContentsObserver(WebContents::FromRenderFrameHost(rfh)),
        render_frame_host_(rfh),
        scheme_(scheme),
        allowed_hosts_(std::move(allowed_hosts)) {
    DCHECK(render_frame_host_);
  }

  ~WebUIURLLoaderFactory() override {}

  void AddReceiver(mojo::PendingReceiver<network::mojom::URLLoaderFactory>
                       factory_receiver) {
    loader_factory_receivers_.Add(this, std::move(factory_receiver));
  }

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

    if (request.url.scheme() != scheme_) {
      DVLOG(1) << "Bad scheme: " << request.url.scheme();
      ReceivedBadMessage(render_frame_host_->GetProcess(),
                         bad_message::WEBUI_BAD_SCHEME_ACCESS);
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
      ReceivedBadMessage(render_frame_host_->GetProcess(),
                         bad_message::WEBUI_BAD_HOST_ACCESS);
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    if (request.url.host_piece() == kChromeUIBlobInternalsHost) {
      base::PostTask(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&StartBlobInternalsURLLoader, request,
                         std::move(client),
                         base::Unretained(ChromeBlobStorageContext::GetFor(
                             GetStoragePartition()->browser_context()))));
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
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &StartURLLoader, request, render_frame_host_->GetFrameTreeNodeId(),
            std::move(client),
            GetStoragePartition()->browser_context()->GetResourceContext()));
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    loader_factory_receivers_.Add(this, std::move(receiver));
  }

  // WebContentsObserver implementation:
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    if (render_frame_host != render_frame_host_)
      return;
    g_web_ui_url_loader_factories.Get().erase(
        GlobalFrameRoutingId(render_frame_host_->GetRoutingID(),
                             render_frame_host_->GetProcess()->GetID()));
  }

  const std::string& scheme() const { return scheme_; }

 private:
  StoragePartitionImpl* GetStoragePartition() {
    return static_cast<StoragePartitionImpl*>(
        render_frame_host_->GetProcess()->GetStoragePartition());
  }

  RenderFrameHost* render_frame_host_;
  std::string scheme_;
  const base::flat_set<std::string> allowed_hosts_;  // if empty all allowed.
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> loader_factory_receivers_;

  DISALLOW_COPY_AND_ASSIGN(WebUIURLLoaderFactory);
};

}  // namespace

std::unique_ptr<network::mojom::URLLoaderFactory> CreateWebUIURLLoader(
    RenderFrameHost* render_frame_host,
    const std::string& scheme,
    base::flat_set<std::string> allowed_hosts) {
  return std::make_unique<WebUIURLLoaderFactory>(render_frame_host, scheme,
                                                 std::move(allowed_hosts));
}

void CreateWebUIURLLoaderBinding(
    RenderFrameHost* render_frame_host,
    const std::string& scheme,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver) {
  GlobalFrameRoutingId routing_id(render_frame_host->GetRoutingID(),
                                  render_frame_host->GetProcess()->GetID());
  if (g_web_ui_url_loader_factories.Get().find(routing_id) ==
          g_web_ui_url_loader_factories.Get().end() ||
      g_web_ui_url_loader_factories.Get()[routing_id]->scheme() != scheme) {
    g_web_ui_url_loader_factories.Get()[routing_id] =
        std::make_unique<WebUIURLLoaderFactory>(render_frame_host, scheme,
                                                base::flat_set<std::string>());
  }
  g_web_ui_url_loader_factories.Get()[routing_id]->AddReceiver(
      std::move(factory_receiver));
}

}  // namespace content
