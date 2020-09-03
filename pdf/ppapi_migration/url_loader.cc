// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/url_loader.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
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
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"

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

UrlLoader::UrlLoader() = default;
UrlLoader::~UrlLoader() = default;

BlinkUrlLoader::BlinkUrlLoader(base::WeakPtr<Client> client)
    : client_(std::move(client)) {}

BlinkUrlLoader::~BlinkUrlLoader() = default;

void BlinkUrlLoader::GrantUniversalAccess() {
  DCHECK(!blink_loader_);
  grant_universal_access_ = true;
}

// Modeled on `content::PepperURLLoaderHost::OnHostMsgOpen()`.
void BlinkUrlLoader::Open(const UrlRequest& request, ResultCallback callback) {
  if (!client_) {
    std::move(callback).Run(PP_ERROR_FAILED);
    return;
  }

  blink::WebAssociatedURLLoaderOptions options;
  options.grant_universal_access = grant_universal_access_;
  blink_loader_ = client_->CreateAssociatedURLLoader(options);
  if (!blink_loader_) {
    std::move(callback).Run(PP_ERROR_FAILED);
    return;
  }

  NOTIMPLEMENTED();
}

bool BlinkUrlLoader::GetDownloadProgress(
    int64_t& bytes_received,
    int64_t& total_bytes_to_be_received) const {
  NOTIMPLEMENTED();
  return false;
}

void BlinkUrlLoader::ReadResponseBody(base::span<char> buffer,
                                      ResultCallback callback) {
  NOTIMPLEMENTED();
}

void BlinkUrlLoader::Close() {
  NOTIMPLEMENTED();
}

bool BlinkUrlLoader::WillFollowRedirect(
    const blink::WebURL& new_url,
    const blink::WebURLResponse& redirect_response) {
  NOTIMPLEMENTED();
  return false;
}

void BlinkUrlLoader::DidSendData(uint64_t bytes_sent,
                                 uint64_t total_bytes_to_be_sent) {
  // Doesn't apply to PDF viewer requests.
  NOTREACHED();
}

void BlinkUrlLoader::DidReceiveResponse(const blink::WebURLResponse& response) {
  NOTIMPLEMENTED();
}

void BlinkUrlLoader::DidDownloadData(uint64_t data_length) {
  // Doesn't apply to PDF viewer requests.
  NOTREACHED();
}

void BlinkUrlLoader::DidReceiveData(const char* data, int data_length) {
  NOTIMPLEMENTED();
}

void BlinkUrlLoader::DidReceiveCachedMetadata(const char* data,
                                              int data_length) {
  // Doesn't apply to PDF viewer requests.
  NOTREACHED();
}

void BlinkUrlLoader::DidFinishLoading() {
  NOTIMPLEMENTED();
}

void BlinkUrlLoader::DidFail(const blink::WebURLError& error) {
  NOTIMPLEMENTED();
}

PepperUrlLoader::PepperUrlLoader(pp::InstanceHandle plugin_instance)
    : plugin_instance_(plugin_instance), pepper_loader_(plugin_instance) {}

PepperUrlLoader::~PepperUrlLoader() = default;

void PepperUrlLoader::GrantUniversalAccess() {
  const PPB_URLLoaderTrusted* trusted_interface =
      static_cast<const PPB_URLLoaderTrusted*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_URLLOADERTRUSTED_INTERFACE));
  if (trusted_interface)
    trusted_interface->GrantUniversalAccess(pepper_loader_.pp_resource());
}

void PepperUrlLoader::Open(const UrlRequest& request, ResultCallback callback) {
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
      base::BindOnce(&PepperUrlLoader::DidOpen, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
  int32_t result = pepper_loader_.Open(pp_request, pp_callback);
  if (result != PP_OK_COMPLETIONPENDING)
    pp_callback.Run(result);
}

bool PepperUrlLoader::GetDownloadProgress(
    int64_t& bytes_received,
    int64_t& total_bytes_to_be_received) const {
  return pepper_loader_.GetDownloadProgress(&bytes_received,
                                            &total_bytes_to_be_received);
}

void PepperUrlLoader::ReadResponseBody(base::span<char> buffer,
                                       ResultCallback callback) {
  pp::CompletionCallback pp_callback =
      PPCompletionCallbackFromResultCallback(std::move(callback));
  int32_t result = pepper_loader_.ReadResponseBody(buffer.data(), buffer.size(),
                                                   pp_callback);
  if (result != PP_OK_COMPLETIONPENDING)
    pp_callback.Run(result);
}

void PepperUrlLoader::Close() {
  pepper_loader_.Close();
}

void PepperUrlLoader::DidOpen(ResultCallback callback, int32_t result) {
  pp::URLResponseInfo pp_response = pepper_loader_.GetResponseInfo();
  mutable_response().status_code = pp_response.GetStatusCode();

  pp::Var headers_var = pp_response.GetHeaders();
  if (headers_var.is_string()) {
    mutable_response().headers = headers_var.AsString();
  } else {
    mutable_response().headers.reset();
  }

  std::move(callback).Run(result);
}

}  // namespace chrome_pdf
