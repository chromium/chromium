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
#include "ipc/ipc_message.h"
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
  base::EraseIf(headers->GetList(), [&predicate](const base::Value& v) {
    return predicate.Run(v.FindKey(keys::kHeaderNameKey)->GetString());
  });
}

}  // namespace

WebRequestEventDetails::WebRequestEventDetails(const WebRequestInfo& request,
                                               int extra_info_spec)
    : extra_info_spec_(extra_info_spec),
      render_process_id_(content::ChildProcessHost::kInvalidUniqueID),
      render_frame_id_(MSG_ROUTING_NONE) {
  dict_.SetString(keys::kMethodKey, request.method);
  dict_.SetString(keys::kRequestIdKey, base::NumberToString(request.id));
  dict_.SetDouble(keys::kTimeStampKey, base::Time::Now().ToDoubleT() * 1000);
  dict_.SetString(keys::kTypeKey,
                  WebRequestResourceTypeToString(request.web_request_type));
  dict_.SetString(keys::kUrlKey, request.url.spec());
  dict_.SetInteger(keys::kTabIdKey, request.frame_data.tab_id);
  dict_.SetInteger(keys::kFrameIdKey, request.frame_data.frame_id);
  dict_.SetInteger(keys::kParentFrameIdKey, request.frame_data.parent_frame_id);
  initiator_ = request.initiator;
  render_process_id_ = request.render_process_id;
  render_frame_id_ = request.frame_id;
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
    headers->Append(helpers::CreateHeaderDictionary(it.name(), it.value()));
  request_headers_.reset(headers);
}

void WebRequestEventDetails::SetAuthInfo(
    const net::AuthChallengeInfo& auth_info) {
  dict_.SetBoolean(keys::kIsProxyKey, auth_info.is_proxy);
  if (!auth_info.scheme.empty())
    dict_.SetString(keys::kSchemeKey, auth_info.scheme);
  if (!auth_info.realm.empty())
    dict_.SetString(keys::kRealmKey, auth_info.realm);
  auto challenger = std::make_unique<base::DictionaryValue>();
  challenger->SetString(keys::kHostKey, auth_info.challenger.host());
  challenger->SetInteger(keys::kPortKey, auth_info.challenger.port());
  dict_.Set(keys::kChallengerKey, std::move(challenger));
}

void WebRequestEventDetails::SetResponseHeaders(
    const WebRequestInfo& request,
    const net::HttpResponseHeaders* response_headers) {
  if (!response_headers) {
    // Not all URLRequestJobs specify response headers. E.g. URLRequestFTPJob,
    // URLRequestFileJob and some redirects.
    dict_.SetInteger(keys::kStatusCodeKey, request.response_code);
    dict_.SetString(keys::kStatusLineKey, "");
  } else {
    dict_.SetInteger(keys::kStatusCodeKey, response_headers->response_code());
    dict_.SetString(keys::kStatusLineKey, response_headers->GetStatusLine());
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
        headers->Append(helpers::CreateHeaderDictionary(name, value));
      }
    }
    response_headers_.reset(headers);
  }
}

void WebRequestEventDetails::SetResponseSource(const WebRequestInfo& request) {
  dict_.SetBoolean(keys::kFromCache, request.response_from_cache);
  if (!request.response_ip.empty())
    dict_.SetString(keys::kIpKey, request.response_ip);
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

  // Only listeners with a permission for the initiator should recieve it.
  if (initiator_) {
    int tab_id = -1;
    dict_.GetInteger(keys::kTabIdKey, &tab_id);
    if (initiator_->opaque() ||
        WebRequestPermissions::CanExtensionAccessInitiator(
            permission_helper, extension_id, initiator_, tab_id,
            crosses_incognito)) {
      result->SetString(keys::kInitiatorKey, initiator_->Serialize());
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

std::unique_ptr<WebRequestEventDetails>
WebRequestEventDetails::CreatePublicSessionCopy() {
  std::unique_ptr<WebRequestEventDetails> copy(new WebRequestEventDetails);
  copy->initiator_ = initiator_;
  copy->render_process_id_ = render_process_id_;
  copy->render_frame_id_ = render_frame_id_;

  static const char* const kSafeAttributes[] = {
    "method", "requestId", "timeStamp", "type", "tabId", "frameId",
    "parentFrameId", "fromCache", "error", "ip", "statusLine", "statusCode"
  };

  for (const char* safe_attr : kSafeAttributes) {
    base::Value* val = dict_.FindKey(safe_attr);
    if (val)
      copy->dict_.SetKey(safe_attr, val->Clone());
  }

  // URL is stripped down to the origin.
  std::string url;
  dict_.GetString(keys::kUrlKey, &url);
  GURL gurl(url);
  copy->dict_.SetString(keys::kUrlKey, gurl.GetOrigin().spec());

  return copy;
}

WebRequestEventDetails::WebRequestEventDetails()
    : extra_info_spec_(0), render_process_id_(0), render_frame_id_(0) {}

}  // namespace extensions
