// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_COMMON_REQUEST_SENDER_H_
#define GOOGLE_APIS_COMMON_REQUEST_SENDER_H_

#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "google_apis/common/api_error_codes.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace base {
class SequencedTaskRunner;
}

namespace google_apis {

class AuthenticatedRequestInterface;
class AuthServiceInterface;

// Helper class that sends requests implementing
// AuthenticatedRequestInterface and handles retries and authentication.
class RequestSender {
 public:
  // |auth_service| is used for fetching OAuth tokens.
  //
  // |url_loader_factory| is the factory used to load resources requested by
  // this RequestSender.
  //
  // |blocking_task_runner| is used for running blocking operation, e.g.,
  // parsing JSON response from the server.
  //
  // |custom_user_agent| will be used for the User-Agent header in HTTP
  // requests issued through the request sender if the value is not empty.
  RequestSender(
      std::unique_ptr<AuthServiceInterface> auth_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
      const std::string& custom_user_agent,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  RequestSender(const RequestSender&) = delete;
  RequestSender& operator=(const RequestSender&) = delete;
  ~RequestSender();

  AuthServiceInterface* auth_service() { return auth_service_.get(); }

  network::SharedURLLoaderFactory* url_loader_factory() const {
    return url_loader_factory_.get();
  }

  base::SequencedTaskRunner* blocking_task_runner() const {
    return blocking_task_runner_.get();
  }

  // Starts a request implementing the AuthenticatedRequestInterface
  // interface, and makes the request retry upon authentication failures by
  // calling back to RetryRequest.
  //
  // Returns a closure to cancel the request. The closure cancels the request
  // if it is in-flight, and does nothing if it is already terminated.
  base::RepeatingClosure StartRequestWithAuthRetry(
      std::unique_ptr<AuthenticatedRequestInterface> request);

  // Notifies to this RequestSender that |request| has finished.
  // TODO(kinaba): refactor the life time management and make this at private.
  void RequestFinished(AuthenticatedRequestInterface* request);

  // Returns traffic annotation tag asssigned to this object.
  const net::NetworkTrafficAnnotationTag& get_traffic_annotation_tag() const {
    return traffic_annotation_;
  }

 private:
  base::RepeatingClosure StartRequestWithAuthRetryInternal(
      AuthenticatedRequestInterface* request);

  // Called when the access token is fetched.
  void OnAccessTokenFetched(
      const base::WeakPtr<AuthenticatedRequestInterface>& request,
      ApiErrorCode error,
      const std::string& access_token);

  // Clears any authentication token and retries the request, which forces
  // an authentication token refresh.
  void RetryRequest(AuthenticatedRequestInterface* request);

  // Cancels the request. Used for implementing the returned closure of
  // StartRequestWithAuthRetry.
  void CancelRequest(
      const base::WeakPtr<AuthenticatedRequestInterface>& request);

  std::unique_ptr<AuthServiceInterface> auth_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  std::set<std::unique_ptr<AuthenticatedRequestInterface>,
           base::UniquePtrComparator>
      in_flight_requests_;
  const std::string custom_user_agent_;

  base::ThreadChecker thread_checker_;

  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RequestSender> weak_ptr_factory_{this};
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_COMMON_REQUEST_SENDER_H_
