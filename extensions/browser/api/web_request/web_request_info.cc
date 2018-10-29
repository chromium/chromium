// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_info.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/values.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/websocket_handshake_request_info.h"
#include "extensions/browser/api/web_request/upload_data_presenter.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/url_loader.h"

namespace keys = extension_web_request_api_constants;

namespace extensions {

namespace {

// UploadDataSource abstracts an interface for feeding an arbitrary data element
// to an UploadDataPresenter. This is helpful because in the Network Service vs
// non-Network Service case, upload data comes from different types of source
// objects, but we'd like to share parsing code.
class UploadDataSource {
 public:
  virtual ~UploadDataSource() {}

  virtual void FeedToPresenter(UploadDataPresenter* presenter) = 0;
};

class BytesUploadDataSource : public UploadDataSource {
 public:
  BytesUploadDataSource(const base::StringPiece& bytes) : bytes_(bytes) {}
  ~BytesUploadDataSource() override = default;

  // UploadDataSource:
  void FeedToPresenter(UploadDataPresenter* presenter) override {
    presenter->FeedBytes(bytes_);
  }

 private:
  base::StringPiece bytes_;

  DISALLOW_COPY_AND_ASSIGN(BytesUploadDataSource);
};

class FileUploadDataSource : public UploadDataSource {
 public:
  FileUploadDataSource(const base::FilePath& path) : path_(path) {}
  ~FileUploadDataSource() override = default;

  // UploadDataSource:
  void FeedToPresenter(UploadDataPresenter* presenter) override {
    presenter->FeedFile(path_);
  }

 private:
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(FileUploadDataSource);
};

std::unique_ptr<base::Value> NetLogExtensionIdCallback(
    const std::string& extension_id,
    net::NetLogCaptureMode capture_mode) {
  auto params = std::make_unique<base::DictionaryValue>();
  params->SetString("extension_id", extension_id);
  return params;
}

// Implements Logger using NetLog, mirroring the logging facilities used prior
// to the introduction of WebRequestInfo.
// TODO(crbug.com/721414): Transition away from using NetLog.
class NetLogLogger : public WebRequestInfo::Logger {
 public:
  explicit NetLogLogger(net::URLRequest* request) : request_(request) {}
  ~NetLogLogger() override = default;

  // WebRequestInfo::Logger:
  void LogEvent(net::NetLogEventType event_type,
                const std::string& extension_id) override {
    request_->net_log().AddEvent(
        event_type,
        base::BindRepeating(&NetLogExtensionIdCallback, extension_id));
  }

  void LogBlockedBy(const std::string& blocker_info) override {
    // LogAndReport allows extensions that block requests to be displayed in the
    // load status bar.
    request_->LogAndReportBlockedBy(blocker_info.c_str());
  }

  void LogUnblocked() override { request_->LogUnblocked(); }

 private:
  net::URLRequest* const request_;

  DISALLOW_COPY_AND_ASSIGN(NetLogLogger);
};

// TODO(https://crbug.com/721414): Need a real implementation here to support
// the Network Service case. For now this is only to prevent crashing.
class NetworkServiceLogger : public WebRequestInfo::Logger {
 public:
  NetworkServiceLogger() = default;
  ~NetworkServiceLogger() override = default;

  // WebRequestInfo::Logger:
  void LogEvent(net::NetLogEventType event_type,
                const std::string& extension_id) override {}
  void LogBlockedBy(const std::string& blocker_info) override {}
  void LogUnblocked() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkServiceLogger);
};

bool CreateUploadDataSourcesFromURLRequest(
    net::URLRequest* url_request,
    std::vector<std::unique_ptr<UploadDataSource>>* data_sources) {
  const net::UploadDataStream* upload_data = url_request->get_upload();
  if (!upload_data)
    return false;

  const std::vector<std::unique_ptr<net::UploadElementReader>>* readers =
      upload_data->GetElementReaders();
  if (!readers)
    return true;

  for (const auto& reader : *readers) {
    if (const auto* bytes_reader = reader->AsBytesReader()) {
      data_sources->push_back(std::make_unique<BytesUploadDataSource>(
          base::StringPiece(bytes_reader->bytes(), bytes_reader->length())));
    } else if (const auto* file_reader = reader->AsFileReader()) {
      data_sources->push_back(
          std::make_unique<FileUploadDataSource>(file_reader->path()));
    } else {
      DVLOG(1) << "Ignoring upsupported upload data type for WebRequest API.";
    }
  }

  return true;
}

bool CreateUploadDataSourcesFromResourceRequest(
    const network::ResourceRequest& request,
    std::vector<std::unique_ptr<UploadDataSource>>* data_sources) {
  if (!request.request_body)
    return false;

  for (auto& element : *request.request_body->elements()) {
    switch (element.type()) {
      case network::DataElement::TYPE_DATA_PIPE:
        // TODO(https://crbug.com/721414): Support data pipe elements.
        break;

      case network::DataElement::TYPE_BYTES:
        data_sources->push_back(std::make_unique<BytesUploadDataSource>(
            base::StringPiece(element.bytes(), element.length())));
        break;

      case network::DataElement::TYPE_FILE:
        // TODO(https://crbug.com/715679): This may not work when network
        // process is sandboxed.
        data_sources->push_back(
            std::make_unique<FileUploadDataSource>(element.path()));
        break;

      default:
        NOTIMPLEMENTED();
        break;
    }
  }

  return true;
}

std::unique_ptr<base::DictionaryValue> CreateRequestBodyData(
    const std::string& method,
    const net::HttpRequestHeaders& request_headers,
    const std::vector<std::unique_ptr<UploadDataSource>>& data_sources) {
  if (method != "POST" && method != "PUT")
    return nullptr;

  auto request_body_data = std::make_unique<base::DictionaryValue>();

  // Get the data presenters, ordered by how specific they are.
  ParsedDataPresenter parsed_data_presenter(request_headers);
  RawDataPresenter raw_data_presenter;
  UploadDataPresenter* const presenters[] = {
      &parsed_data_presenter,  // 1: any parseable forms? (Specific to forms.)
      &raw_data_presenter      // 2: any data at all? (Non-specific.)
  };
  // Keys for the results of the corresponding presenters.
  static const char* const kKeys[] = {keys::kRequestBodyFormDataKey,
                                      keys::kRequestBodyRawKey};
  bool some_succeeded = false;
  if (!data_sources.empty()) {
    for (size_t i = 0; i < arraysize(presenters); ++i) {
      for (auto& source : data_sources)
        source->FeedToPresenter(presenters[i]);
      if (presenters[i]->Succeeded()) {
        request_body_data->Set(kKeys[i], presenters[i]->Result());
        some_succeeded = true;
        break;
      }
    }
  }

  if (!some_succeeded)
    request_body_data->SetString(keys::kRequestBodyErrorKey, "Unknown error.");

  return request_body_data;
}

}  // namespace

WebRequestInfo::WebRequestInfo() = default;
WebRequestInfo::WebRequestInfo(WebRequestInfo&& other) = default;
WebRequestInfo& WebRequestInfo::operator=(WebRequestInfo&& other) = default;

WebRequestInfo::WebRequestInfo(net::URLRequest* url_request)
    : id(url_request->identifier()),
      url(url_request->url()),
      site_for_cookies(url_request->site_for_cookies()),
      method(url_request->method()),
      initiator(url_request->initiator()),
      extra_request_headers(url_request->extra_request_headers()),
      is_pac_request(url_request->is_pac_request()),
      logger(std::make_unique<NetLogLogger>(url_request)) {
  if (url.SchemeIsWSOrWSS()) {
    web_request_type = WebRequestResourceType::WEB_SOCKET;

    // TODO(pkalinnikov): Consider embedding WebSocketHandshakeRequestInfo into
    // UrlRequestUserData.
    const content::WebSocketHandshakeRequestInfo* ws_info =
        content::WebSocketHandshakeRequestInfo::ForRequest(url_request);
    if (ws_info) {
      render_process_id = ws_info->GetChildId();
      frame_id = ws_info->GetRenderFrameId();
    }
  } else if (auto* info =
                 content::ResourceRequestInfo::ForRequest(url_request)) {
    render_process_id = info->GetChildID();
    routing_id = info->GetRouteID();
    frame_id = info->GetRenderFrameID();
    type = info->GetResourceType();
    web_request_type = ToWebRequestResourceType(type.value());
    is_async = info->IsAsync();
    resource_context = info->GetContext();
  } else if (auto* url_loader = network::URLLoader::ForRequest(*url_request)) {
    // This is reached only in the SimpleURLLoader case (since network service
    // is disabled if we're in this constructor). Only set the IDs if they're
    // non-zero, since almost all requests come from the browser and aren't
    // associated with a frame. In the case that the browser wants this
    // SimpleURLLoader associated with a frame, the process ID will be non-zero.
    if (url_loader->GetProcessId() != network::mojom::kBrowserProcessId) {
      render_process_id = url_loader->GetProcessId();
      frame_id = url_loader->GetRenderFrameId();
    }
  } else {
    // There may be basic process and frame info associated with the request
    // even when |info| is null. Attempt to grab it as a last ditch effort. If
    // this fails, we have no frame info.
    content::ResourceRequestInfo::GetRenderFrameForRequest(
        url_request, &render_process_id, &frame_id);
  }

  ExtensionsBrowserClient* browser_client = ExtensionsBrowserClient::Get();
  ExtensionNavigationUIData* navigation_ui_data =
      browser_client ? browser_client->GetExtensionNavigationUIData(url_request)
                     : nullptr;
  if (navigation_ui_data)
    is_browser_side_navigation = true;

  InitializeWebViewAndFrameData(navigation_ui_data);

  std::vector<std::unique_ptr<UploadDataSource>> data_sources;
  if (CreateUploadDataSourcesFromURLRequest(url_request, &data_sources)) {
    request_body_data =
        CreateRequestBodyData(method, extra_request_headers, data_sources);
  }
}

WebRequestInfo::WebRequestInfo(
    uint64_t request_id,
    int render_process_id,
    int render_frame_id,
    std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data,
    int32_t routing_id,
    content::ResourceContext* resource_context,
    const network::ResourceRequest& request,
    bool is_async)
    : id(request_id),
      url(request.url),
      site_for_cookies(request.site_for_cookies),
      render_process_id(render_process_id),
      routing_id(routing_id),
      frame_id(render_frame_id),
      method(request.method),
      is_browser_side_navigation(!!navigation_ui_data),
      initiator(request.request_initiator),
      type(static_cast<content::ResourceType>(request.resource_type)),
      is_async(is_async),
      extra_request_headers(request.headers),
      logger(std::make_unique<NetworkServiceLogger>()),
      resource_context(resource_context) {
  if (url.SchemeIsWSOrWSS())
    web_request_type = WebRequestResourceType::WEB_SOCKET;
  else
    web_request_type = ToWebRequestResourceType(type.value());

  InitializeWebViewAndFrameData(navigation_ui_data.get());

  std::vector<std::unique_ptr<UploadDataSource>> data_sources;
  if (CreateUploadDataSourcesFromResourceRequest(request, &data_sources)) {
    request_body_data =
        CreateRequestBodyData(method, extra_request_headers, data_sources);
  }

  // TODO(https://crbug.com/721414): For this constructor (i.e. the Network
  // Service case), we are still missing information for |is_async| and
  // |is_pac_request|.
}

WebRequestInfo::~WebRequestInfo() = default;

void WebRequestInfo::AddResponseInfoFromURLRequest(
    net::URLRequest* url_request) {
  response_code = url_request->GetResponseCode();
  response_headers = url_request->response_headers();
  response_ip = url_request->GetSocketAddress().host();
  response_from_cache = url_request->was_cached();
}

void WebRequestInfo::AddResponseInfoFromResourceResponse(
    const network::ResourceResponseHead& response) {
  response_headers = response.headers;
  if (response_headers)
    response_code = response_headers->response_code();
  response_ip = response.socket_address.host();
  response_from_cache = response.was_fetched_via_cache;
}

void WebRequestInfo::InitializeWebViewAndFrameData(
    const ExtensionNavigationUIData* navigation_ui_data) {
  if (navigation_ui_data) {
    is_web_view = navigation_ui_data->is_web_view();
    web_view_instance_id = navigation_ui_data->web_view_instance_id();
    web_view_rules_registry_id =
        navigation_ui_data->web_view_rules_registry_id();
    frame_data = navigation_ui_data->frame_data();
  } else if (frame_id >= 0) {
    // Grab any WebView-related information if relevant.
    WebViewRendererState::WebViewInfo web_view_info;
    if (WebViewRendererState::GetInstance()->GetInfo(
            render_process_id, routing_id, &web_view_info)) {
      is_web_view = true;
      web_view_instance_id = web_view_info.instance_id;
      web_view_rules_registry_id = web_view_info.rules_registry_id;
      web_view_embedder_process_id = web_view_info.embedder_process_id;
    }

    // For subresource loads we attempt to resolve the FrameData immediately
    // anyway using cached information.
    ExtensionApiFrameIdMap::FrameData data;
    bool was_cached = ExtensionApiFrameIdMap::Get()->GetCachedFrameDataOnIO(
        render_process_id, frame_id, &data);
    // TODO(crbug.com/843762): Investigate when |was_cached| can be false. It
    // seems we are not tracking all WebContents or that the corresponding
    // render frame was destroyed. Track where this can occur, this should help
    // in minimizing IO->UI->IO thread that the web request API performs to
    // fetch the frame data.
    if (was_cached)
      frame_data = data;
  }
}

}  // namespace extensions
