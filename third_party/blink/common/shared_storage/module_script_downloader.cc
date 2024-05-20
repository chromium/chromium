// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/shared_storage/module_script_downloader.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "url/gurl.h"

namespace blink {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "shared_storage_worklet_module_script_downloader",
        R"(
        semantics {
          sender: "ModuleScriptDownloader"
          description:
            "Requests the module script for shared storage worklet."
          trigger:
            "Requested when window.sharedStorage.worklet.addModule() is "
            "invoked. This is an API that any website can call."
          data: "URL of the script."
          destination: WEBSITE
          internal {
            contacts {
              email: "yaoxia@google.com"
            }
            contacts {
              email: "cammie@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2023-03-01"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "These requests can be disabled in chrome://settings/privacySandbox."
          policy_exception_justification:
            "These requests are triggered by a website."
        })");

// Checks if `charset` is a valid charset, in lowercase ASCII. Takes `body` as
// well, to ensure it uses the specified charset.
bool IsAllowedCharset(std::string_view charset, const std::string& body) {
  if (charset == "utf-8" || charset.empty()) {
    return base::IsStringUTF8(body);
  } else if (charset == "us-ascii") {
    return base::IsStringASCII(body);
  }
  // TODO(yaoxia): Worth supporting iso-8859-1, or full character set list?
  return false;
}

}  // namespace

ModuleScriptDownloader::ModuleScriptDownloader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& source_url,
    ModuleScriptDownloaderCallback module_script_downloader_callback)
    : source_url_(source_url),
      module_script_downloader_callback_(
          std::move(module_script_downloader_callback)) {
  DCHECK(module_script_downloader_callback_);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = source_url;

  // These fields are ignored, but mirror the browser-side behavior to be safe.
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kSameOrigin;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAccept,
      std::string_view("application/javascript"));

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);

  // Abort on redirects.
  // TODO(yaoxia): May want a browser-side proxy to block redirects instead.
  simple_url_loader_->SetOnRedirectCallback(base::BindRepeating(
      &ModuleScriptDownloader::OnRedirect, base::Unretained(this)));

  // TODO(yaoxia): Consider limiting the size of response bodies.
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&ModuleScriptDownloader::OnBodyReceived,
                     base::Unretained(this)));
}

ModuleScriptDownloader::~ModuleScriptDownloader() = default;

void ModuleScriptDownloader::OnBodyReceived(std::unique_ptr<std::string> body) {
  DCHECK(module_script_downloader_callback_);

  auto simple_url_loader = std::move(simple_url_loader_);

  if (!body) {
    std::string error_message;
    if (simple_url_loader->ResponseInfo() &&
        simple_url_loader->ResponseInfo()->headers &&
        simple_url_loader->ResponseInfo()->headers->response_code() / 100 !=
            2) {
      int status = simple_url_loader->ResponseInfo()->headers->response_code();
      error_message = base::StringPrintf(
          "Failed to load %s HTTP status = %d %s.", source_url_.spec().c_str(),
          status,
          simple_url_loader->ResponseInfo()->headers->GetStatusText().c_str());
    } else {
      error_message = base::StringPrintf(
          "Failed to load %s error = %s.", source_url_.spec().c_str(),
          net::ErrorToString(simple_url_loader->NetError()).c_str());
    }
    std::move(module_script_downloader_callback_)
        .Run(/*body=*/nullptr, error_message,
             simple_url_loader->TakeResponseInfo());
    return;
  }

  if (!blink::IsSupportedJavascriptMimeType(
          simple_url_loader->ResponseInfo()->mime_type)) {
    std::move(module_script_downloader_callback_)
        .Run(/*body=*/nullptr,
             base::StringPrintf(
                 "Rejecting load of %s due to unexpected MIME type.",
                 source_url_.spec().c_str()),
             simple_url_loader->TakeResponseInfo());
    return;
  }

  if (!IsAllowedCharset(simple_url_loader->ResponseInfo()->charset, *body)) {
    std::move(module_script_downloader_callback_)
        .Run(/*body=*/nullptr,
             base::StringPrintf(
                 "Rejecting load of %s due to unexpected charset.",
                 source_url_.spec().c_str()),
             simple_url_loader->TakeResponseInfo());
    return;
  }

  // All OK!
  std::move(module_script_downloader_callback_)
      .Run(std::move(body), /*error_message=*/{},
           simple_url_loader->TakeResponseInfo());
}

void ModuleScriptDownloader::OnRedirect(
    const GURL& url_before_redirect,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  DCHECK(module_script_downloader_callback_);

  // Need to cancel the load, to prevent the request from continuing.
  simple_url_loader_.reset();

  std::move(module_script_downloader_callback_)
      .Run(/*body=*/nullptr,
           base::StringPrintf("Unexpected redirect on %s.",
                              source_url_.spec().c_str()),
           nullptr);
}

}  // namespace blink
