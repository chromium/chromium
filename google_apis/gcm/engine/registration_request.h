// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_REGISTRATION_REQUEST_H_
#define GOOGLE_APIS_GCM_ENGINE_REGISTRATION_REQUEST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

namespace net {
class HttpRequestHeaders;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace gcm {

class GCMStatsRecorder;

// Registration request is used to obtain registration IDs for applications that
// want to use GCM. It requires a set of parameters to be specified to identify
// the Chrome instance, the user, the application and a set of senders that will
// be authorized to address the application using its assigned registration ID.
class GCM_EXPORT RegistrationRequest {
 public:
  // This enum is also used in an UMA histogram (GCMRegistrationRequestStatus
  // enum defined in tools/metrics/histograms/histogram.xml). Hence the entries
  // here shouldn't be deleted or re-ordered and new ones should be added to
  // the end.
  enum Status {
    SUCCESS,                    // Registration completed successfully.
    INVALID_PARAMETERS,         // One of request paramteres was invalid.
    INVALID_SENDER,             // One of the provided senders was invalid.
    AUTHENTICATION_FAILED,      // Authentication failed.
    DEVICE_REGISTRATION_ERROR,  // Chrome is not properly registered.
    UNKNOWN_ERROR,              // Unknown error.
    URL_FETCHING_FAILED,        // URL fetching failed.
    HTTP_NOT_OK,                // HTTP status was not OK.
    NO_RESPONSE_BODY,           // No response body.
    REACHED_MAX_RETRIES,        // Reached maximum number of retries.
    RESPONSE_PARSING_FAILED,    // Registration response parsing failed.
    INTERNAL_SERVER_ERROR,      // Internal server error during request.
    QUOTA_EXCEEDED,             // Registration quota exceeded.
    TOO_MANY_REGISTRATIONS,     // Max registrations per device exceeded.
    // NOTE: always keep this entry at the end. Add new status types only
    // immediately above this line. Make sure to update the corresponding
    // histogram enum accordingly.
    STATUS_COUNT
  };

  // Callback completing the registration request.
  using RegistrationCallback =
      base::OnceCallback<void(Status status,
                              const std::string& registration_id)>;

  // Defines the common info about a registration/token request. All parameters
  // are mandatory.
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
    const std::string& app_id() const {
      return subtype.empty() ? category : subtype;
    }

    // GCM category field.
    std::string category;
    // GCM subtype field.
    std::string subtype;
  };

  // Encapsulates the custom logic that is needed to build and process the
  // registration request.
  class GCM_EXPORT CustomRequestHandler {
   public:
    CustomRequestHandler();
    virtual ~CustomRequestHandler();

    // Builds the HTTP request body data. It is called after
    // RegistrationRequest::BuildRequestBody to append more custom info to
    // |body|. Note that the request body is encoded in HTTP form format.
    virtual void BuildRequestBody(std::string* body) = 0;

    // Reports the status of a request.
    virtual void ReportStatusToUMA(Status status) = 0;

    // Reports the net error code from a request.
    virtual void ReportNetErrorCodeToUMA(int net_error_code) = 0;
  };

  RegistrationRequest(
      const GURL& registration_url,
      const RequestInfo& request_info,
      std::unique_ptr<CustomRequestHandler> custom_request_handler,
      const net::BackoffEntry::Policy& backoff_policy,
      RegistrationCallback callback,
      int max_retry_count,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      GCMStatsRecorder* recorder,
      const std::string& source_to_record);
  ~RegistrationRequest();

  void Start();

  // Invoked from SimpleURLLoader.
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> body);

 private:
  // Schedules a retry attempt with a backoff.
  void RetryWithBackoff();

  void BuildRequestHeaders(net::HttpRequestHeaders* headers);
  void BuildRequestBody(std::string* body);

  // Parse the response returned by the URL loader into token, and returns the
  // status.
  Status ParseResponse(const network::SimpleURLLoader* source,
                       std::unique_ptr<std::string> body,
                       std::string* token);

  RegistrationCallback callback_;
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

  base::WeakPtrFactory<RegistrationRequest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RegistrationRequest);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_REGISTRATION_REQUEST_H_
