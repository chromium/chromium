// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/request_sender.h"

#include <utility>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "google_apis/drive/auth_service.h"
#include "google_apis/drive/base_requests.h"

namespace google_apis {

RequestSender::RequestSender(
    std::unique_ptr<AuthServiceInterface> auth_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    const std::string& custom_user_agent,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : auth_service_(std::move(auth_service)),
      url_loader_factory_(url_loader_factory),
      blocking_task_runner_(blocking_task_runner),
      custom_user_agent_(custom_user_agent),
      traffic_annotation_(traffic_annotation) {}

RequestSender::~RequestSender() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

base::Closure RequestSender::StartRequestWithAuthRetry(
    std::unique_ptr<AuthenticatedRequestInterface> request) {
  DCHECK(thread_checker_.CalledOnValidThread());

  AuthenticatedRequestInterface* request_ptr = request.get();
  in_flight_requests_.insert(std::move(request));

  return StartRequestWithAuthRetryInternal(request_ptr);
}

base::Closure RequestSender::StartRequestWithAuthRetryInternal(
    AuthenticatedRequestInterface* request) {
  // TODO(kinaba): Stop relying on weak pointers. Move lifetime management
  // of the requests to request sender.
  base::Closure cancel_closure =
      base::Bind(&RequestSender::CancelRequest, weak_ptr_factory_.GetWeakPtr(),
                 request->GetWeakPtr());

  if (!auth_service_->HasAccessToken()) {
    // Fetch OAuth2 access token from the refresh token first.
    auth_service_->StartAuthentication(
        base::Bind(&RequestSender::OnAccessTokenFetched,
                   weak_ptr_factory_.GetWeakPtr(), request->GetWeakPtr()));
  } else {
    request->Start(auth_service_->access_token(), custom_user_agent_,
                   base::Bind(&RequestSender::RetryRequest,
                              weak_ptr_factory_.GetWeakPtr()));
  }

  return cancel_closure;
}

void RequestSender::OnAccessTokenFetched(
    const base::WeakPtr<AuthenticatedRequestInterface>& request,
    DriveApiErrorCode code,
    const std::string& /* access_token */) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Do nothing if the request is canceled during authentication.
  if (!request.get())
    return;

  if (code == HTTP_SUCCESS) {
    DCHECK(auth_service_->HasAccessToken());
    StartRequestWithAuthRetryInternal(request.get());
  } else {
    request->OnAuthFailed(code);
  }
}

void RequestSender::RetryRequest(AuthenticatedRequestInterface* request) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auth_service_->ClearAccessToken();
  // User authentication might have expired - rerun the request to force
  // auth token refresh.
  StartRequestWithAuthRetryInternal(request);
}

void RequestSender::CancelRequest(
    const base::WeakPtr<AuthenticatedRequestInterface>& request) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Do nothing if the request is already finished.
  if (!request.get())
    return;
  request->Cancel();
}

void RequestSender::RequestFinished(AuthenticatedRequestInterface* request) {
  auto it = in_flight_requests_.find(request);
  if (it == in_flight_requests_.end()) {
    // Various BatchUpload tests in DriveApiRequestsTest will commit requests
    // using this RequestSender without actually starting them on it. In that
    // case, there's nothing to be done, so just return.
    return;
  }

  in_flight_requests_.erase(it);
}

}  // namespace google_apis
