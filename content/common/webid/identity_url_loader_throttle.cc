// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/webid/identity_url_loader_throttle.h"

#include <algorithm>
#include <string_view>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/common/features.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom.h"
#include "url/origin.h"

using blink::mojom::IdpSigninStatus;

namespace {
static constexpr char kSetLoginHeader[] = "Set-Login";

static constexpr char kSetLoginHeaderValueLoggedIn[] = "logged-in";
static constexpr char kSetLoginHeaderValueLoggedOut[] = "logged-out";

}  // namespace

namespace content {

ParseSetLoginHeaderCallback GetSetLoginHeaderInProcessParser() {
  return base::BindRepeating(
      [](const std::string& header_value,
         base::OnceCallback<void(
             std::optional<net::structured_headers::ParameterizedItem> item)>
             callback) {
        std::move(callback).Run(
            net::structured_headers::ParseItem(header_value));
      });
}

ParseSetLoginHeaderCallback GetSetLoginHeaderDataDecoderParser() {
  return base::BindRepeating(
      [](const std::string& header_value,
         base::OnceCallback<void(
             std::optional<net::structured_headers::ParameterizedItem> item)>
             callback) {
        data_decoder::DataDecoder::ParseStructuredHeaderItemIsolated(
            header_value,
            base::BindOnce(
                [](base::OnceCallback<void(
                       std::optional<net::structured_headers::ParameterizedItem>
                           item)> cb,
                   base::expected<net::structured_headers::ParameterizedItem,
                                  std::string> result) {
                  if (result.has_value()) {
                    std::move(cb).Run(std::move(*result));
                  } else {
                    std::move(cb).Run(std::nullopt);
                  }
                },
                std::move(callback)));
      });
}

std::unique_ptr<blink::URLLoaderThrottle> MaybeCreateIdentityUrlLoaderThrottle(
    SetIdpStatusCallback status_cb,
    ParseSetLoginHeaderCallback parse_cb) {
  return std::make_unique<IdentityUrlLoaderThrottle>(std::move(status_cb),
                                                     std::move(parse_cb));
}

IdentityUrlLoaderThrottle::IdentityUrlLoaderThrottle(
    SetIdpStatusCallback status_cb,
    ParseSetLoginHeaderCallback parse_cb)
    : set_idp_status_cb_(std::move(status_cb)),
      parse_set_login_header_cb_(std::move(parse_cb)) {}

IdentityUrlLoaderThrottle::~IdentityUrlLoaderThrottle() = default;

void IdentityUrlLoaderThrottle::DetachFromCurrentSequence() {
  // This gets called when the load is moved to a different thread, so we need
  // to post a task to the original thread to set the signin status.
  // See https://crbug.com/40285364 and https://crbug.com/40244488.
  set_idp_status_cb_ = base::BindRepeating(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         SetIdpStatusCallback original_cb,
         const std::optional<url::Origin>& initiator,
         const url::Origin& idp_origin, blink::mojom::IdpSigninStatus status) {
        task_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(original_cb), initiator,
                                             idp_origin, status));
      },
      base::SequencedTaskRunner::GetCurrentDefault(),
      std::move(set_idp_status_cb_));
}

void IdentityUrlLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  request_url_ = request->url;
  request_initiator_ = request->request_initiator;
}

void IdentityUrlLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  DCHECK(response_head);
  HandleResponseOrRedirect(response_url, *response_head, defer);
}

void IdentityUrlLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    network::HttpRequestHeadersUpdateParams* headers_update_params) {
  // We want to check headers for each redirect. It is common that the header
  // is on the initial load which then redirects back to a homepage.
  HandleResponseOrRedirect(request_url_, response_head, defer);
  request_url_ = redirect_info->new_url;
}

void IdentityUrlLoaderThrottle::HandleResponseOrRedirect(
    const GURL& response_url,
    const network::mojom::URLResponseHead& response_head,
    bool* defer) {
  url::Origin idp_origin = url::Origin::Create(response_url);
  if (!network::IsOriginPotentiallyTrustworthy(idp_origin)) {
    return;
  }

  // TODO(crbug.com/40236764):
  // - Limit to toplevel frames
  // - Decide whether to limit to same-origin

  scoped_refptr<net::HttpResponseHeaders> headers = response_head.headers;
  if (!headers)
    return;

  std::optional<std::string> header_value =
      headers->GetNormalizedHeader(kSetLoginHeader);
  if (!header_value) {
    return;
  }

  CHECK(parse_set_login_header_cb_);
  is_header_parsed_ = false;
  {
    base::AutoReset<bool> auto_reset(&is_inside_handler_response_, true);
    parse_set_login_header_cb_.Run(
        *header_value,
        base::BindOnce(&IdentityUrlLoaderThrottle::OnHeaderParsed,
                       weak_ptr_factory_.GetWeakPtr(), idp_origin));
  }

  // If header is not parsed yet, then the parsing callback is running
  // asynchronously and we need to defer.
  *defer = !is_header_parsed_;
}

void IdentityUrlLoaderThrottle::OnHeaderParsed(
    const url::Origin& idp_origin,
    std::optional<net::structured_headers::ParameterizedItem> item) {
  is_header_parsed_ = true;

  if (item && item->item.is_token()) {
    const std::string& token = item->item.GetString();
    if (token == kSetLoginHeaderValueLoggedIn) {
      // Mark IDP as logged in
      VLOG(1) << "IDP signed in: " << idp_origin.Serialize();
      set_idp_status_cb_.Run(request_initiator_, idp_origin,
                             IdpSigninStatus::kSignedIn);
    } else if (token == kSetLoginHeaderValueLoggedOut) {
      // Mark IDP as logged out
      VLOG(1) << "IDP signed out: " << idp_origin.Serialize();
      set_idp_status_cb_.Run(request_initiator_, idp_origin,
                             IdpSigninStatus::kSignedOut);
    }
  }

  if (!is_inside_handler_response_) {
    if (delegate_) {
      delegate_->Resume();
    }
  }
}

}  // namespace content
