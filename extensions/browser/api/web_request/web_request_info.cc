// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/web_request/web_request_info.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/websocket_handshake_request_info.h"
#include "extensions/browser/api/web_request/upload_data_presenter.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "net/base/ip_endpoint.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/url_loader.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

namespace keys = extension_web_request_api_constants;

namespace extensions {

namespace {

// UploadDataSource abstracts an interface for feeding an arbitrary data element
// to an UploadDataPresenter.
class UploadDataSource {
 public:
  virtual ~UploadDataSource() {}

  virtual void FeedToPresenter(UploadDataPresenter* presenter) = 0;
};

class BytesUploadDataSource : public UploadDataSource {
 public:
  BytesUploadDataSource(std::string_view bytes) : bytes_(bytes) {}

  BytesUploadDataSource(const BytesUploadDataSource&) = delete;
  BytesUploadDataSource& operator=(const BytesUploadDataSource&) = delete;

  ~BytesUploadDataSource() override = default;

  // UploadDataSource:
  void FeedToPresenter(UploadDataPresenter* presenter) override {
    presenter->FeedBytes(bytes_);
  }

 private:
  std::string_view bytes_;
};

class FileUploadDataSource : public UploadDataSource {
 public:
  FileUploadDataSource(const base::FilePath& path) : path_(path) {}

  FileUploadDataSource(const FileUploadDataSource&) = delete;
  FileUploadDataSource& operator=(const FileUploadDataSource&) = delete;

  ~FileUploadDataSource() override = default;

  // UploadDataSource:
  void FeedToPresenter(UploadDataPresenter* presenter) override {
    presenter->FeedFile(path_);
  }

 private:
  base::FilePath path_;
};

bool CreateUploadDataSourcesFromResourceRequest(
    const network::ResourceRequest& request,
    std::vector<std::unique_ptr<UploadDataSource>>* data_sources) {
  if (!request.request_body) {
    return false;
  }

  for (auto& element : *request.request_body->elements()) {
    switch (element.type()) {
      case network::DataElement::Tag::kDataPipe:
        // TODO(crbug.com/41318947): Support data pipe elements.
        break;

      case network::DataElement::Tag::kBytes:
        data_sources->push_back(std::make_unique<BytesUploadDataSource>(
            element.As<network::DataElementBytes>().AsStringPiece()));
        break;
      case network::DataElement::Tag::kFile:
        // TODO(crbug.com/41315406): This may not work when network
        // process is sandboxed.
        data_sources->push_back(std::make_unique<FileUploadDataSource>(
            element.As<network::DataElementFile>().path()));
        break;

      default:
        NOTIMPLEMENTED();
        break;
    }
  }

  return true;
}

std::optional<base::Value::Dict> CreateRequestBodyData(
    const std::string& method,
    const net::HttpRequestHeaders& request_headers,
    const std::vector<std::unique_ptr<UploadDataSource>>& data_sources) {
  if (method != "POST" && method != "PUT") {
    return std::nullopt;
  }

  base::Value::Dict request_body_data;

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
    for (size_t i = 0; i < std::size(presenters); ++i) {
      for (auto& source : data_sources) {
        source->FeedToPresenter(presenters[i]);
      }
      if (presenters[i]->Succeeded()) {
        request_body_data.Set(kKeys[i], presenters[i]->TakeResult().value());
        some_succeeded = true;
        break;
      }
    }
  }

  if (!some_succeeded) {
    request_body_data.Set(keys::kRequestBodyErrorKey, "Unknown error.");
  }

  return request_body_data;
}

}  // namespace

WebRequestInfoInitParams::WebRequestInfoInitParams() = default;

WebRequestInfoInitParams::WebRequestInfoInitParams(
    uint64_t request_id,
    int render_process_id,
    int frame_routing_id,
    std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data,
    const network::ResourceRequest& request,
    bool is_download,
    bool is_async,
    bool is_service_worker_script,
    std::optional<int64_t> navigation_id)
    : id(request_id),
      url(request.url),
      render_process_id(render_process_id),
      frame_routing_id(frame_routing_id),
      method(request.method),
      is_navigation_request(!!navigation_ui_data),
      initiator(request.request_initiator),
      is_async(is_async),
      extra_request_headers(request.headers),
      is_service_worker_script(is_service_worker_script),
      navigation_id(std::move(navigation_id)) {
  web_request_type = ToWebRequestResourceType(request, is_download);

  DCHECK_EQ(is_navigation_request, this->navigation_id.has_value());

  InitializeWebViewAndFrameData(navigation_ui_data.get());

  std::vector<std::unique_ptr<UploadDataSource>> data_sources;
  if (CreateUploadDataSourcesFromResourceRequest(request, &data_sources)) {
    request_body_data =
        CreateRequestBodyData(method, extra_request_headers, data_sources);
  }
}

WebRequestInfoInitParams::WebRequestInfoInitParams(
    WebRequestInfoInitParams&& other) = default;

WebRequestInfoInitParams& WebRequestInfoInitParams::operator=(
    WebRequestInfoInitParams&& other) = default;

WebRequestInfoInitParams::~WebRequestInfoInitParams() = default;

void WebRequestInfoInitParams::InitializeWebViewAndFrameData(
    const ExtensionNavigationUIData* navigation_ui_data) {
  if (navigation_ui_data) {
    is_web_view = navigation_ui_data->is_web_view();
    if (is_web_view) {
      web_view_instance_id = navigation_ui_data->web_view_instance_id();
      web_view_rules_registry_id =
          navigation_ui_data->web_view_rules_registry_id();
    }
    frame_data = navigation_ui_data->frame_data();
    parent_routing_id = navigation_ui_data->parent_routing_id();
  } else if (frame_routing_id != MSG_ROUTING_NONE) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
    // Grab any WebView-related information if relevant.
    WebViewRendererState::WebViewInfo web_view_info;
    if (WebViewRendererState::GetInstance()->GetInfo(
            render_process_id, frame_routing_id, &web_view_info)) {
      is_web_view = true;
      web_view_instance_id = web_view_info.instance_id;
      web_view_rules_registry_id = web_view_info.rules_registry_id;
      web_view_embedder_process_id = web_view_info.embedder_process_id;
    }
#endif

    parent_routing_id =
        content::GlobalRenderFrameHostId(render_process_id, frame_routing_id);

    // For subresource loads we attempt to resolve the FrameData immediately.
    frame_data = ExtensionApiFrameIdMap::Get()->GetFrameData(parent_routing_id);
  }
}

WebRequestInfo::WebRequestInfo(WebRequestInfoInitParams params)
    : id(params.id),
      url(std::move(params.url)),
      render_process_id(params.render_process_id),
      frame_routing_id(params.frame_routing_id),
      method(std::move(params.method)),
      is_navigation_request(params.is_navigation_request),
      initiator(std::move(params.initiator)),
      frame_data(std::move(params.frame_data)),
      web_request_type(params.web_request_type),
      is_async(params.is_async),
      extra_request_headers(std::move(params.extra_request_headers)),
      request_body_data(std::move(params.request_body_data)),
      is_web_view(params.is_web_view),
      web_view_instance_id(params.web_view_instance_id),
      web_view_rules_registry_id(params.web_view_rules_registry_id),
      web_view_embedder_process_id(params.web_view_embedder_process_id),
      is_service_worker_script(params.is_service_worker_script),
      navigation_id(std::move(params.navigation_id)),
      parent_routing_id(params.parent_routing_id) {}

WebRequestInfo::~WebRequestInfo() = default;

void WebRequestInfo::AddResponseInfoFromResourceResponse(
    const network::mojom::URLResponseHead& response) {
  response_headers = response.headers;
  if (response_headers) {
    response_code = response_headers->response_code();
  }
  response_ip = response.remote_endpoint.ToStringWithoutPort();
  response_from_cache = response.was_fetched_via_cache;
}

void WebRequestInfo::EraseDNRActionsForExtension(
    const ExtensionId& extension_id) {
  if (dnr_actions.has_value()) {
    std::erase_if(
        *dnr_actions,
        [extension_id](declarative_net_request::RequestAction& action) {
          return action.extension_id == extension_id;
        });
  }
}

void WebRequestInfo::EraseOutprioritizedDNRActions() {
  if (dnr_actions.has_value()) {
    std::erase_if(
        *dnr_actions,
        [this](const declarative_net_request::RequestAction& action) {
          // Check that the cache contains the action's extension ID
          // to make sure that rule matching was performed for that
          // extension.
          DCHECK(
              base::Contains(max_priority_allow_action, action.extension_id));

          uint64_t allow_rule_priority_for_extension = 0u;
          if (auto& allow_action =
                  max_priority_allow_action.at(action.extension_id)) {
            allow_rule_priority_for_extension = allow_action->index_priority;
          }

          return action.index_priority < allow_rule_priority_for_extension;
        });
  }
}

bool WebRequestInfo::ShouldRecordMatchedAllowRuleInOnHeadersReceived(
    const declarative_net_request::RequestAction& allow_action) const {
  CHECK(dnr_actions.has_value());

  // If there are no more relevant actions, `allow_action` should be matched.
  if (dnr_actions->empty()) {
    return true;
  }

  // If the only actions matched modify request headers, then `allow_action`
  // should match since said actions are no longer relevant in
  // onHeadersReceived.
  bool only_request_headers_modified =
      base::ranges::all_of(*dnr_actions, [](const auto& action) {
        return action.type == declarative_net_request::RequestAction::Type::
                                  MODIFY_HEADERS &&
               action.response_headers_to_modify.empty();
      });
  if (only_request_headers_modified) {
    return true;
  }

  // Compare allow actions.
  if ((*dnr_actions)[0].IsAllowOrAllowAllRequests()) {
    CHECK_EQ(1u, dnr_actions->size());

    // If `allow_action` is from a different extension than `dnr_actions`, then
    // we record a match since `allow action` could've prevented other action
    // types from its extension from matching.
    // If `allow_action` is from the same extension as `dnr_actions`, then it
    // would've made `dnr_actions` empty through EraseOutprioritizedDNRActions()
    // if it was higher priority, and if not (and this if statement is reached),
    // it shouldn't be matched since `dnr_actions` still takes precedence.
    return allow_action.extension_id != (*dnr_actions)[0].extension_id;
  }

  return false;
}

}  // namespace extensions
