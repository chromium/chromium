// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_UNREGISTRATION_REQUEST_H_
#define GOOGLE_APIS_GCM_ENGINE_UNREGISTRATION_REQUEST_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

namespace net {
class HttpRequestHeaders;
}

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}

namespace gcm {

class GCMStatsRecorder;

// Encapsulates the common logic applying to both GCM unregistration requests
// and InstanceID delete-token requests. In case an attempt fails, it will retry
// using the backoff policy.
// TODO(fgorski): Consider sharing code with RegistrationRequest if possible.
class GCM_EXPORT UnregistrationRequest {
 public:
  // Outcome of the response parsing. Note that these enums are consumed by a
  // histogram, so ordering should not be modified.
  enum Status {
    SUCCESS,                  // Unregistration completed successfully.
    URL_FETCHING_FAILED,      // URL fetching failed.
    NO_RESPONSE_BODY,         // No response body.
    RESPONSE_PARSING_FAILED,  // Failed to parse a meaningful output from
                              // response body.
    INCORRECT_APP_ID,         // App ID returned by the fetcher does not match
                              // request.
    INVALID_PARAMETERS,       // Request parameters were invalid.
    SERVICE_UNAVAILABLE,      // Unregistration service unavailable.
    INTERNAL_SERVER_ERROR,    // Internal server error happened during request.
    HTTP_NOT_OK,              // HTTP response code was not OK.
    UNKNOWN_ERROR,            // Unknown error.
    REACHED_MAX_RETRIES,      // Reached maximum number of retries.
    DEVICE_REGISTRATION_ERROR,// Chrome is not properly registered.
    // NOTE: Always keep this entry at the end. Add new status types only
    // immediately above this line. Make sure to update the corresponding
    // histogram enum accordingly.
    UNREGISTRATION_STATUS_COUNT,
  };

  // Callback completing the unregistration request.
  typedef base::Callback<void(Status success)> UnregistrationCallback;

  // Defines the common info about an unregistration/token-deletion request.
  // All parameters are mandatory.
  struct GCM_EXPORT RequestInfo {
    RequestInfo(uint64_t android_id,
                uint64_t security_token,
                const std::string& category,
                const std::string& subtype);
    ~RequestInfo();

    // Android ID of the device.
    uint64_t android_id;
    // Security token of the device.
    uint64_t security_token;

    // Application ID used in Chrome to refer to registration/token's owner.
    const std::string& app_id() { return subtype.empty() ? category : subtype; }

    // GCM category field derived from the |app_id|.
    std::string category;
    // GCM subtype field derived from the |app_id|.
    std::string subtype;
  };

  // Encapsulates the custom logic that is needed to build and process the
  // unregistration request.
  class GCM_EXPORT CustomRequestHandler {
   public:
    CustomRequestHandler();
    virtual ~CustomRequestHandler();

    // Builds the HTTP request body data. It is called after
    // UnregistrationRequest::BuildRequestBody to append more custom info to
    // |body|. Note that the request body is encoded in HTTP form format.
    virtual void BuildRequestBody(std::string* body) = 0;

    // Parses the HTTP response. It is called after
    // UnregistrationRequest::ParseResponse to proceed the parsing.
    virtual Status ParseResponse(const std::string& response) = 0;

    // Reports UMAs.
    virtual void ReportUMAs(Status status) = 0;
  };

  // Creates an instance of UnregistrationRequest. |callback| will be called
  // once registration has been revoked or there has been an error that makes
  // further retries pointless.
  UnregistrationRequest(
      const GURL& registration_url,
      const RequestInfo& request_info,
      std::unique_ptr<CustomRequestHandler> custom_request_handler,
      const net::BackoffEntry::Policy& backoff_policy,
      const UnregistrationCallback& callback,
      int max_retry_count,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      GCMStatsRecorder* recorder,
      const std::string& source_to_record);
  ~UnregistrationRequest();

  // Starts an unregistration request.
  void Start();

 private:
  // Invoked from SimpleURLLoader.
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> body);

  void BuildRequestHeaders(net::HttpRequestHeaders* headers);
  void BuildRequestBody(std::string* body);
  Status ParseResponse(const network::SimpleURLLoader* source,
                       std::unique_ptr<std::string> body);

  // Schedules a retry attempt with a backoff.
  void RetryWithBackoff();

  UnregistrationCallback callback_;
  RequestInfo request_info_;
  std::unique_ptr<CustomRequestHandler> custom_request_handler_;
  GURL registration_url_;

  net::BackoffEntry backoff_entry_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  int retries_left_;

  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // Recorder that records GCM activities for debugging purpose. Not owned.
  GCMStatsRecorder* recorder_;
  std::string source_to_record_;

  base::WeakPtrFactory<UnregistrationRequest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UnregistrationRequest);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_UNREGISTRATION_REQUEST_H_
