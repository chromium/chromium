// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_event_details.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/upload_data_presenter.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_permissions.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/auth.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"

using extension_web_request_api_helpers::ExtraInfoSpec;

namespace helpers = extension_web_request_api_helpers;
namespace keys = extension_web_request_api_constants;

namespace extensions {
namespace {

// Removes all headers for which predicate(header_name) returns true.
void EraseHeadersIf(
    base::Value::List& headers,
    base::RepeatingCallback<bool(const std::string&)> predicate) {
  headers.EraseIf([&predicate](const base::Value& v) {
    return predicate.Run(*v.GetDict().FindString(keys::kHeaderNameKey));
  });
}

}  // namespace

WebRequestEventDetails::WebRequestEventDetails(const WebRequestInfo& request,
                                               int extra_info_spec)
    : extra_info_spec_(extra_info_spec),
      render_process_id_(content::ChildProcessHost::kInvalidUniqueID) {
  dict_.Set(keys::kMethodKey, request.method);
  dict_.Set(keys::kRequestIdKey, base::NumberToString(request.id));
  dict_.Set(keys::kTimeStampKey,
            base::Time::Now().InMillisecondsFSinceUnixEpoch());
  dict_.Set(keys::kTypeKey,
            WebRequestResourceTypeToString(request.web_request_type));
  dict_.Set(keys::kUrlKey, request.url.spec());
  dict_.Set(keys::kTabIdKey, request.frame_data.tab_id);
  dict_.Set(keys::kFrameIdKey, request.frame_data.frame_id);
  dict_.Set(keys::kParentFrameIdKey, request.frame_data.parent_frame_id);
  if (request.frame_data.document_id) {
    dict_.Set(keys::kDocumentIdKey, request.frame_data.document_id.ToString());
  }
  if (request.frame_data.parent_document_id) {
    dict_.Set(keys::kParentDocumentIdKey,
              request.frame_data.parent_document_id.ToString());
  }
  if (request.frame_data.frame_id >= 0) {
    dict_.Set(keys::kFrameTypeKey, ToString(request.frame_data.frame_type));
    dict_.Set(keys::kDocumentLifecycleKey,
              ToString(request.frame_data.document_lifecycle));
  }
  initiator_ = request.initiator;
  render_process_id_ = request.render_process_id;
}

WebRequestEventDetails::~WebRequestEventDetails() = default;

void WebRequestEventDetails::SetRequestBody(WebRequestInfo* request) {
  if (!(extra_info_spec_ & ExtraInfoSpec::REQUEST_BODY)) {
    return;
  }
  request_body_ = std::nullopt;
  if (request->request_body_data) {
    request_body_ = std::move(request->request_body_data);
    request->request_body_data.reset();
  }
}

void WebRequestEventDetails::SetRequestHeaders(
    const net::HttpRequestHeaders& request_headers) {
  if (!(extra_info_spec_ & ExtraInfoSpec::REQUEST_HEADERS)) {
    return;
  }

  request_headers_ = base::Value::List();
  for (net::HttpRequestHeaders::Iterator it(request_headers); it.GetNext();) {
    request_headers_->Append(
        helpers::CreateHeaderDictionary(it.name(), it.value()));
  }
}

void WebRequestEventDetails::SetAuthInfo(
    const net::AuthChallengeInfo& auth_info) {
  dict_.Set(keys::kIsProxyKey, auth_info.is_proxy);
  if (!auth_info.scheme.empty()) {
    dict_.Set(keys::kSchemeKey, auth_info.scheme);
  }
  if (!auth_info.realm.empty()) {
    dict_.Set(keys::kRealmKey, auth_info.realm);
  }
  base::Value::Dict challenger;
  challenger.Set(keys::kHostKey, auth_info.challenger.host());
  challenger.Set(keys::kPortKey, auth_info.challenger.port());
  dict_.Set(keys::kChallengerKey, std::move(challenger));
}

void WebRequestEventDetails::SetResponseHeaders(
    const WebRequestInfo& request,
    const net::HttpResponseHeaders* response_headers) {
  if (!response_headers) {
    // Not all URLRequestJobs specify response headers. E.g. URLRequestFTPJob,
    // URLRequestFileJob and some redirects.
    dict_.Set(keys::kStatusCodeKey, request.response_code);
    dict_.Set(keys::kStatusLineKey, "");
  } else {
    dict_.Set(keys::kStatusCodeKey, response_headers->response_code());
    dict_.Set(keys::kStatusLineKey, response_headers->GetStatusLine());
  }

  if (extra_info_spec_ & ExtraInfoSpec::RESPONSE_HEADERS) {
    response_headers_ = base::Value::List();
    if (response_headers) {
      size_t iter = 0;
      std::string name;
      std::string value;
      while (response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
        if (ExtensionsAPIClient::Get()->ShouldHideResponseHeader(request.url,
                                                                 name)) {
          continue;
        }
        response_headers_->Append(helpers::CreateHeaderDictionary(name, value));
      }
    }
  }
}

void WebRequestEventDetails::SetResponseSource(const WebRequestInfo& request) {
  dict_.Set(keys::kFromCache, request.response_from_cache);
  if (!request.response_ip.empty()) {
    dict_.Set(keys::kIpKey, request.response_ip);
  }
}

base::Value::Dict WebRequestEventDetails::GetFilteredDict(
    int extra_info_spec,
    PermissionHelper* permission_helper,
    const extensions::ExtensionId& extension_id,
    bool crosses_incognito) const {
  base::Value::Dict result = dict_.Clone();
  if ((extra_info_spec & ExtraInfoSpec::REQUEST_BODY) && request_body_) {
    result.Set(keys::kRequestBodyKey, request_body_->Clone());
  }
  if ((extra_info_spec & ExtraInfoSpec::REQUEST_HEADERS) && request_headers_) {
    content::RenderProcessHost* process =
        content::RenderProcessHost::FromID(render_process_id_);
    content::BrowserContext* browser_context =
        process ? process->GetBrowserContext() : nullptr;
    base::Value::List request_headers = request_headers_->Clone();
    EraseHeadersIf(request_headers,
                   base::BindRepeating(helpers::ShouldHideRequestHeader,
                                       browser_context, extra_info_spec));
    result.Set(keys::kRequestHeadersKey, std::move(request_headers));
  }
  if ((extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS) &&
      response_headers_) {
    base::Value::List response_headers = response_headers_->Clone();
    EraseHeadersIf(response_headers,
                   base::BindRepeating(helpers::ShouldHideResponseHeader,
                                       extra_info_spec));
    result.Set(keys::kResponseHeadersKey, std::move(response_headers));
  }

  // Only listeners with a permission for the initiator should receive it.
  if (initiator_) {
    int tab_id = dict_.FindInt(keys::kTabIdKey).value_or(-1);
    if (initiator_->opaque() ||
        WebRequestPermissions::CanExtensionAccessInitiator(
            permission_helper, extension_id, initiator_, tab_id,
            crosses_incognito)) {
      result.Set(keys::kInitiatorKey, initiator_->Serialize());
    }
  }
  return result;
}

base::Value::Dict WebRequestEventDetails::GetAndClearDict() {
  return std::move(dict_);
}

}  // namespace extensions
