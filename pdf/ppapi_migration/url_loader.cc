// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/url_loader.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "pdf/ppapi_migration/callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/cpp/var.h"

namespace chrome_pdf {

UrlRequest::UrlRequest() = default;
UrlRequest::UrlRequest(const UrlRequest& other) = default;
UrlRequest::UrlRequest(UrlRequest&& other) noexcept = default;
UrlRequest& UrlRequest::operator=(const UrlRequest& other) = default;
UrlRequest& UrlRequest::operator=(UrlRequest&& other) noexcept = default;
UrlRequest::~UrlRequest() = default;

UrlResponse::UrlResponse() = default;
UrlResponse::UrlResponse(const UrlResponse& other) = default;
UrlResponse::UrlResponse(UrlResponse&& other) noexcept = default;
UrlResponse& UrlResponse::operator=(const UrlResponse& other) = default;
UrlResponse& UrlResponse::operator=(UrlResponse&& other) noexcept = default;
UrlResponse::~UrlResponse() = default;

UrlLoader::UrlLoader() : plugin_instance_(0) {}

UrlLoader::UrlLoader(pp::InstanceHandle plugin_instance)
    : plugin_instance_(plugin_instance), pepper_loader_(plugin_instance) {}

UrlLoader::~UrlLoader() = default;

void UrlLoader::GrantUniversalAccess() {
  const PPB_URLLoaderTrusted* trusted_interface =
      static_cast<const PPB_URLLoaderTrusted*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_URLLOADERTRUSTED_INTERFACE));
  if (trusted_interface)
    trusted_interface->GrantUniversalAccess(pepper_loader_.pp_resource());
}

void UrlLoader::Open(const UrlRequest& request, ResultCallback callback) {
  pp::URLRequestInfo pp_request(plugin_instance_);
  pp_request.SetURL(request.url);
  pp_request.SetMethod(request.method);

  if (request.ignore_redirects)
    pp_request.SetFollowRedirects(false);

  if (request.custom_referrer_url.has_value())
    pp_request.SetCustomReferrerURL(request.custom_referrer_url.value());

  if (request.headers.has_value())
    pp_request.SetHeaders(request.headers.value());

  if (!request.body.empty())
    pp_request.AppendDataToBody(request.body.data(), request.body.size());

  pp::CompletionCallback pp_callback = PPCompletionCallbackFromResultCallback(
      base::BindOnce(&UrlLoader::DidOpen, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
  int32_t result = pepper_loader_.Open(pp_request, pp_callback);
  if (result != PP_OK_COMPLETIONPENDING)
    pp_callback.Run(result);
}

bool UrlLoader::GetDownloadProgress(int64_t& bytes_received,
                                    int64_t& total_bytes_to_be_received) const {
  return pepper_loader_.GetDownloadProgress(&bytes_received,
                                            &total_bytes_to_be_received);
}

void UrlLoader::ReadResponseBody(base::span<char> buffer,
                                 ResultCallback callback) {
  pp::CompletionCallback pp_callback =
      PPCompletionCallbackFromResultCallback(std::move(callback));
  int32_t result = pepper_loader_.ReadResponseBody(buffer.data(), buffer.size(),
                                                   pp_callback);
  if (result != PP_OK_COMPLETIONPENDING)
    pp_callback.Run(result);
}

void UrlLoader::Close() {
  pepper_loader_.Close();
}

void UrlLoader::DidOpen(ResultCallback callback, int32_t result) {
  pp::URLResponseInfo pp_response = pepper_loader_.GetResponseInfo();
  response_.status_code = pp_response.GetStatusCode();

  pp::Var headers_var = pp_response.GetHeaders();
  if (headers_var.is_string()) {
    response_.headers = headers_var.AsString();
  } else {
    response_.headers.reset();
  }

  std::move(callback).Run(result);
}

}  // namespace chrome_pdf
