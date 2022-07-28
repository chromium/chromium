// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_event_details.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
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
    base::Value* headers,
    base::RepeatingCallback<bool(const std::string&)> predicate) {
  headers->EraseListValueIf([&predicate](const base::Value& v) {
    return predicate.Run(v.FindKey(keys::kHeaderNameKey)->GetString());
  });
}

}  // namespace

WebRequestEventDetails::WebRequestEventDetails(const WebRequestInfo& request,
                                               int extra_info_spec)
    : extra_info_spec_(extra_info_spec),
      render_process_id_(content::ChildProcessHost::kInvalidUniqueID) {
  dict_.SetStringKey(keys::kMethodKey, request.method);
  dict_.SetStringKey(keys::kRequestIdKey, base::NumberToString(request.id));
  dict_.SetDoubleKey(keys::kTimeStampKey, base::Time::Now().ToDoubleT() * 1000);
  dict_.SetStringKey(keys::kTypeKey,
                     WebRequestResourceTypeToString(request.web_request_type));
  dict_.SetStringKey(keys::kUrlKey, request.url.spec());
  dict_.SetIntKey(keys::kTabIdKey, request.frame_data.tab_id);
  dict_.SetIntKey(keys::kFrameIdKey, request.frame_data.frame_id);
  dict_.SetIntKey(keys::kParentFrameIdKey, request.frame_data.parent_frame_id);
  if (request.frame_data.document_id) {
    dict_.SetStringKey(keys::kDocumentIdKey,
                       request.frame_data.document_id.ToString());
  }
  if (request.frame_data.parent_document_id) {
    dict_.SetStringKey(keys::kParentDocumentIdKey,
                       request.frame_data.parent_document_id.ToString());
  }
  if (request.frame_data.frame_id >= 0) {
    dict_.SetStringKey(keys::kFrameTypeKey,
                       ToString(request.frame_data.frame_type));
    dict_.SetStringKey(keys::kDocumentLifecycleKey,
                       ToString(request.frame_data.document_lifecycle));
  }
  initiator_ = request.initiator;
  render_process_id_ = request.render_process_id;
}

WebRequestEventDetails::~WebRequestEventDetails() = default;

void WebRequestEventDetails::SetRequestBody(WebRequestInfo* request) {
  if (!(extra_info_spec_ & ExtraInfoSpec::REQUEST_BODY))
    return;
  request_body_ = std::move(request->request_body_data);
}

void WebRequestEventDetails::SetRequestHeaders(
    const net::HttpRequestHeaders& request_headers) {
  if (!(extra_info_spec_ & ExtraInfoSpec::REQUEST_HEADERS))
    return;

  base::ListValue* headers = new base::ListValue();
  for (net::HttpRequestHeaders::Iterator it(request_headers); it.GetNext();)
    headers->Append(
        base::Value(helpers::CreateHeaderDictionary(it.name(), it.value())));
  request_headers_.reset(headers);
}

void WebRequestEventDetails::SetAuthInfo(
    const net::AuthChallengeInfo& auth_info) {
  dict_.SetBoolKey(keys::kIsProxyKey, auth_info.is_proxy);
  if (!auth_info.scheme.empty())
    dict_.SetStringKey(keys::kSchemeKey, auth_info.scheme);
  if (!auth_info.realm.empty())
    dict_.SetStringKey(keys::kRealmKey, auth_info.realm);
  base::Value challenger(base::Value::Type::DICTIONARY);
  challenger.SetStringKey(keys::kHostKey, auth_info.challenger.host());
  challenger.SetIntKey(keys::kPortKey, auth_info.challenger.port());
  dict_.SetKey(keys::kChallengerKey, std::move(challenger));
}

void WebRequestEventDetails::SetResponseHeaders(
    const WebRequestInfo& request,
    const net::HttpResponseHeaders* response_headers) {
  if (!response_headers) {
    // Not all URLRequestJobs specify response headers. E.g. URLRequestFTPJob,
    // URLRequestFileJob and some redirects.
    dict_.SetIntKey(keys::kStatusCodeKey, request.response_code);
    dict_.SetStringKey(keys::kStatusLineKey, "");
  } else {
    dict_.SetIntKey(keys::kStatusCodeKey, response_headers->response_code());
    dict_.SetStringKey(keys::kStatusLineKey, response_headers->GetStatusLine());
  }

  if (extra_info_spec_ & ExtraInfoSpec::RESPONSE_HEADERS) {
    base::ListValue* headers = new base::ListValue();
    if (response_headers) {
      size_t iter = 0;
      std::string name;
      std::string value;
      while (response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
        if (ExtensionsAPIClient::Get()->ShouldHideResponseHeader(request.url,
                                                                 name)) {
          continue;
        }
        headers->Append(
            base::Value(helpers::CreateHeaderDictionary(name, value)));
      }
    }
    response_headers_.reset(headers);
  }
}

void WebRequestEventDetails::SetResponseSource(const WebRequestInfo& request) {
  dict_.SetBoolKey(keys::kFromCache, request.response_from_cache);
  if (!request.response_ip.empty())
    dict_.SetStringKey(keys::kIpKey, request.response_ip);
}

std::unique_ptr<base::DictionaryValue> WebRequestEventDetails::GetFilteredDict(
    int extra_info_spec,
    PermissionHelper* permission_helper,
    const extensions::ExtensionId& extension_id,
    bool crosses_incognito) const {
  std::unique_ptr<base::DictionaryValue> result = dict_.CreateDeepCopy();
  if ((extra_info_spec & ExtraInfoSpec::REQUEST_BODY) && request_body_) {
    result->SetKey(keys::kRequestBodyKey, request_body_->Clone());
  }
  if ((extra_info_spec & ExtraInfoSpec::REQUEST_HEADERS) && request_headers_) {
    content::RenderProcessHost* process =
        content::RenderProcessHost::FromID(render_process_id_);
    content::BrowserContext* browser_context =
        process ? process->GetBrowserContext() : nullptr;
    base::Value request_headers = request_headers_->Clone();
    EraseHeadersIf(&request_headers,
                   base::BindRepeating(helpers::ShouldHideRequestHeader,
                                       browser_context, extra_info_spec));
    result->SetKey(keys::kRequestHeadersKey, std::move(request_headers));
  }
  if ((extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS) &&
      response_headers_) {
    base::Value response_headers = response_headers_->Clone();
    EraseHeadersIf(&response_headers,
                   base::BindRepeating(helpers::ShouldHideResponseHeader,
                                       extra_info_spec));
    result->SetKey(keys::kResponseHeadersKey, std::move(response_headers));
  }

  // Only listeners with a permission for the initiator should receive it.
  if (initiator_) {
    int tab_id = dict_.FindIntKey(keys::kTabIdKey).value_or(-1);
    if (initiator_->opaque() ||
        WebRequestPermissions::CanExtensionAccessInitiator(
            permission_helper, extension_id, initiator_, tab_id,
            crosses_incognito)) {
      result->SetStringKey(keys::kInitiatorKey, initiator_->Serialize());
    }
  }
  return result;
}

std::unique_ptr<base::DictionaryValue>
WebRequestEventDetails::GetAndClearDict() {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  dict_.Swap(result.get());
  return result;
}

}  // namespace extensions
