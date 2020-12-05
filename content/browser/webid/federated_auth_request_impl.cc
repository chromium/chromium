// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "url/url_constants.h"

using blink::mojom::RequestIdTokenStatus;

namespace content {

namespace {

// Determines whether |host| is same-origin with all of its ancestors in the
// frame tree. Returns false if not.
// |origin| is provided because it is not considered safe to use
// host->GetLastCommittedOrigin() at some times, so FrameServiceBase::origin()
// should be used to obtain the frame's origin.
bool IsSameOriginWithAncestors(RenderFrameHost* host,
                               const url::Origin& origin) {
  RenderFrameHost* parent = host->GetParent();
  while (parent) {
    if (!parent->GetLastCommittedOrigin().IsSameOriginWith(origin)) {
      return false;
    }
    parent = parent->GetParent();
  }
  return true;
}

// Checks requirements for URLs received from the IDP.
bool IdpUrlIsValid(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme))
    return false;

  return true;
}

}  // namespace

FederatedAuthRequestImpl::FederatedAuthRequestImpl(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver)
    : FrameServiceBase(host, std::move(receiver)) {}

FederatedAuthRequestImpl::~FederatedAuthRequestImpl() = default;

// static
void FederatedAuthRequestImpl::Create(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  DCHECK(host);

  // TODO(kenrb): This should also be verified in the renderer process before
  // the mojo method is invoked, causing the promise to be rejected.
  // https://crbug.com/1141125
  // It is safe to access host->GetLastCommittedOrigin during construction
  // but FrameServiceBase::origin() should be used thereafter.
  if (!IsSameOriginWithAncestors(host, host->GetLastCommittedOrigin())) {
    mojo::ReportBadMessage(
        "WebID cannot be invoked from within cross-origin iframes.");
    return;
  }

  // FederatedAuthRequestImpl owns itself. It will self-destruct when a mojo
  // interface error occurs, the render frame host is deleted, or the render
  // frame host navigates to a new document.
  new FederatedAuthRequestImpl(host, std::move(receiver));
}

void FederatedAuthRequestImpl::RequestIdToken(const GURL& provider,
                                              const std::string& id_request,
                                              RequestIdTokenCallback callback) {
  if (callback_) {
    std::move(callback).Run(RequestIdTokenStatus::kErrorTooManyRequests, "");
    return;
  }

  callback_ = std::move(callback);
  provider_ = provider;
  id_request_ = id_request;

  network_manager_ =
      IdpNetworkRequestManager::Create(provider, render_frame_host());
  if (!network_manager_) {
    std::move(callback_).Run(RequestIdTokenStatus::kError, "");
    return;
  }

  network_manager_->FetchIDPWellKnown(
      base::BindOnce(&FederatedAuthRequestImpl::OnWellKnownFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnWellKnownFetched(
    IdpNetworkRequestManager::FetchStatus status,
    const std::string& idp_endpoint) {
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kWebIdNotSupported: {
      std::move(callback_).Run(
          RequestIdTokenStatus::kErrorWebIdNotSupportedByProvider, "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kFetchError: {
      std::move(callback_).Run(RequestIdTokenStatus::kError, "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      // Intentional fall-through.
    }
  }

  idp_endpoint_url_ = GURL(base::StringPiece(idp_endpoint));
  // TODO(kenrb): Do we have to check that this URL is same-origin with the
  // provider, or is that not a requirement?
  // https://crbug.com/1141125
  if (!IdpUrlIsValid(idp_endpoint_url_)) {
    std::move(callback_).Run(RequestIdTokenStatus::kError, "");
    return;
  }

  // TODO(kenrb): Call out to the consent UI with OnSigninApproved as a
  // callback. https://crbug.com/1141125.
  OnSigninApproved(true);
}

void FederatedAuthRequestImpl::OnSigninApproved(bool approval_granted) {
  if (!approval_granted) {
    std::move(callback_).Run(RequestIdTokenStatus::kApprovalDeclined, "");
    return;
  }

  network_manager_->SendSigninRequest(
      idp_endpoint_url_, id_request_,
      base::BindOnce(&FederatedAuthRequestImpl::OnSigninResponseReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnSigninResponseReceived(
    IdpNetworkRequestManager::SigninResponse status,
    const std::string& response) {
  // |response| is either the URL for the sign-in page or the ID token,
  // depending on |status|.
  switch (status) {
    case IdpNetworkRequestManager::SigninResponse::kLoadIdp: {
      GURL idp_signin_page_url = GURL(base::StringPiece(response));
      if (!IdpUrlIsValid(idp_signin_page_url)) {
        std::move(callback_).Run(RequestIdTokenStatus::kError, "");
        return;
      }
    }
      // TODO(kenrb): Create window and load IDP URL. For now just return
      // success. https://crbug.com/1141125.
      std::move(callback_).Run(RequestIdTokenStatus::kSuccess, "");
      return;
    case IdpNetworkRequestManager::SigninResponse::kTokenGranted: {
      std::move(callback_).Run(RequestIdTokenStatus::kSuccess, response);
      return;
    }
    case IdpNetworkRequestManager::SigninResponse::kSigninError: {
      std::move(callback_).Run(RequestIdTokenStatus::kError, "");
      return;
    }
  }
}

}  // namespace content
