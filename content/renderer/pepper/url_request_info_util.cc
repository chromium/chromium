// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/url_request_info_util.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/public/common/service_names.mojom.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_file_ref_renderer_host.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "net/http/http_util.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/url_request_info_data.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_feature.mojom.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"
#include "url/url_util.h"

using ppapi::Resource;
using ppapi::URLRequestInfoData;
using ppapi::thunk::EnterResourceNoLock;
using blink::WebData;
using blink::WebHTTPBody;
using blink::WebString;
using blink::WebLocalFrame;
using blink::WebURL;
using blink::WebURLRequest;

namespace content {

namespace {

blink::mojom::FileSystemManagerPtr GetFileSystemManager() {
  blink::mojom::FileSystemManagerPtr file_system_manager_ptr;
  ChildThreadImpl::current()->GetConnector()->BindInterface(
      mojom::kBrowserServiceName, mojo::MakeRequest(&file_system_manager_ptr));
  return file_system_manager_ptr;
}

// Appends the file ref given the Resource pointer associated with it to the
// given HTTP body, returning true on success.
bool AppendFileRefToBody(PP_Instance instance,
                         PP_Resource resource,
                         int64_t start_offset,
                         int64_t number_of_bytes,
                         PP_Time expected_last_modified_time,
                         WebHTTPBody* http_body) {
  base::FilePath platform_path;
  PepperPluginInstanceImpl* instance_impl =
      HostGlobals::Get()->GetInstance(instance);
  if (!instance_impl)
    return false;

  RendererPpapiHost* renderer_ppapi_host =
      instance_impl->module()->renderer_ppapi_host();
  if (!renderer_ppapi_host)
    return false;
  ppapi::host::ResourceHost* resource_host =
      renderer_ppapi_host->GetPpapiHost()->GetResourceHost(resource);
  if (!resource_host || !resource_host->IsFileRefHost())
    return false;
  PepperFileRefRendererHost* file_ref_host =
      static_cast<PepperFileRefRendererHost*>(resource_host);
  switch (file_ref_host->GetFileSystemType()) {
    case PP_FILESYSTEMTYPE_LOCALTEMPORARY:
    case PP_FILESYSTEMTYPE_LOCALPERSISTENT:
      // TODO(kinuko): remove this sync IPC when we fully support
      // AppendURLRange for FileSystem URL.
      GetFileSystemManager()->GetPlatformPath(file_ref_host->GetFileSystemURL(),
                                              &platform_path);
      break;
    case PP_FILESYSTEMTYPE_EXTERNAL:
      platform_path = file_ref_host->GetExternalFilePath();
      break;
    default:
      NOTREACHED();
  }
  http_body->AppendFileRange(blink::FilePathToWebString(platform_path),
                             start_offset, number_of_bytes,
                             expected_last_modified_time);
  return true;
}

// Checks that the request data is valid. Returns false on failure. Note that
// method and header validation is done by the URL loader when the request is
// opened, and any access errors are returned asynchronously.
bool ValidateURLRequestData(const URLRequestInfoData& data) {
  if (data.prefetch_buffer_lower_threshold < 0 ||
      data.prefetch_buffer_upper_threshold < 0 ||
      data.prefetch_buffer_upper_threshold <=
          data.prefetch_buffer_lower_threshold) {
    return false;
  }
  return true;
}

std::string FilterStringForXRequestedWithValue(const std::string& s) {
  std::string rv;
  rv.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    // Allow ASCII digits, letters, periods, commas, and underscores. (Ignore
    // all other characters.)
    if (base::IsAsciiDigit(c) || base::IsAsciiAlpha(c) || (c == '.') ||
        (c == ',') || (c == '_'))
      rv.push_back(c);
  }
  return rv;
}

// Returns an appropriate value for the X-Requested-With header for plugins that
// present an X-Requested-With header. Returns a blank string for other plugins.
// We produce a user-agent-like string (eating spaces and other undesired
// characters) like "ShockwaveFlash/11.5.31.135" from the plugin name and
// version.
std::string MakeXRequestedWithValue(const std::string& name,
                                    const std::string& version) {
  std::string rv = FilterStringForXRequestedWithValue(name);
  if (rv.empty())
    return std::string();

  // Apply to a narrow list of plugins only.
  if (rv != "ShockwaveFlash" && rv != "PPAPITests")
    return std::string();

  std::string filtered_version = FilterStringForXRequestedWithValue(version);
  if (!filtered_version.empty())
    rv += "/" + filtered_version;

  return rv;
}

}  // namespace

bool CreateWebURLRequest(PP_Instance instance,
                         URLRequestInfoData* data,
                         WebLocalFrame* frame,
                         WebURLRequest* dest) {
  // In the out-of-process case, we've received the URLRequestInfoData
  // from the untrusted plugin and done no validation on it. We need to be
  // sure it's not being malicious by checking everything for consistency.
  if (!ValidateURLRequestData(*data))
    return false;

  std::string name_version;

  // Allow instance to be 0 or -1 for testing purposes.
  if (instance && instance != -1) {
    PepperPluginInstanceImpl* instance_impl =
        HostGlobals::Get()->GetInstance(instance);
    if (instance_impl) {
      name_version = MakeXRequestedWithValue(
          instance_impl->module()->name(), instance_impl->module()->version());
    }
  } else {
    name_version = "internal_testing_only";
  }

  dest->SetURL(
      frame->GetDocument().CompleteURL(WebString::FromUTF8(data->url)));
  dest->SetReportUploadProgress(data->record_upload_progress);

  if (!data->method.empty())
    dest->SetHTTPMethod(WebString::FromUTF8(data->method));

  dest->SetSiteForCookies(frame->GetDocument().SiteForCookies());

  // Plug-ins should not load via service workers as plug-ins may have their own
  // origin checking logic that may get confused if service workers respond with
  // resources from another origin.
  // https://w3c.github.io/ServiceWorker/#implementer-concerns
  dest->SetSkipServiceWorker(true);

  const std::string& headers = data->headers;
  if (!headers.empty()) {
    net::HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\n\r");
    while (it.GetNext()) {
      dest->AddHTTPHeaderField(WebString::FromUTF8(it.name()),
                               WebString::FromUTF8(it.values()));
    }
  }

  // Append the upload data.
  if (!data->body.empty()) {
    WebHTTPBody http_body;
    http_body.Initialize();
    int file_index = 0;
    for (size_t i = 0; i < data->body.size(); ++i) {
      const URLRequestInfoData::BodyItem& item = data->body[i];
      if (item.is_file) {
        if (!AppendFileRefToBody(instance,
                                 item.file_ref_pp_resource,
                                 item.start_offset,
                                 item.number_of_bytes,
                                 item.expected_last_modified_time,
                                 &http_body))
          return false;
        file_index++;
      } else {
        DCHECK(!item.data.empty());
        http_body.AppendData(WebData(item.data));
      }
    }
    dest->SetHTTPBody(http_body);
  }

  // Add the "Referer" header if there is a custom referrer. Such requests
  // require universal access. For all other requests, "Referer" will be set
  // after header security checks are done in AssociatedURLLoader.
  if (data->has_custom_referrer_url && !data->custom_referrer_url.empty())
    frame->SetReferrerForRequest(*dest, GURL(data->custom_referrer_url));

  if (data->has_custom_content_transfer_encoding &&
      !data->custom_content_transfer_encoding.empty()) {
    dest->AddHTTPHeaderField(
        WebString::FromUTF8("Content-Transfer-Encoding"),
        WebString::FromUTF8(data->custom_content_transfer_encoding));
  }

  if (!name_version.empty())
    dest->SetRequestedWith(WebString::FromUTF8(name_version));

  if (data->has_custom_user_agent) {
    auto extra_data = std::make_unique<RequestExtraData>();
    extra_data->set_custom_user_agent(
        WebString::FromUTF8(data->custom_user_agent));
    dest->SetExtraData(std::move(extra_data));
  }

  return true;
}

bool URLRequestRequiresUniversalAccess(const URLRequestInfoData& data) {
  return data.has_custom_referrer_url ||
         data.has_custom_content_transfer_encoding ||
         data.has_custom_user_agent ||
         url::FindAndCompareScheme(data.url, url::kJavaScriptScheme, nullptr);
}

}  // namespace content
