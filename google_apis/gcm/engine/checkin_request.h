// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_CHECKIN_REQUEST_H_
#define GOOGLE_APIS_GCM_ENGINE_CHECKIN_REQUEST_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/protocol/android_checkin.pb.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "net/base/backoff_entry.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}

namespace gcm {

class GCMStatsRecorder;

// Enables making check-in requests with the GCM infrastructure. When called
// with android_id and security_token both set to 0 it is an initial check-in
// used to obtain credentials. These should be persisted and used for subsequent
// check-ins.
class GCM_EXPORT CheckinRequest {
 public:
  // A callback function for the checkin request, accepting |checkin_response|
  // protobuf.
  typedef base::Callback<void(
      net::HttpStatusCode response_code,
      const checkin_proto::AndroidCheckinResponse& checkin_response)>
      CheckinRequestCallback;

  // Checkin request details.
  struct GCM_EXPORT RequestInfo {
    RequestInfo(uint64_t android_id,
                uint64_t security_token,
                const std::map<std::string, std::string>& account_tokens,
                const std::string& settings_digest,
                const checkin_proto::ChromeBuildProto& chrome_build_proto);
    RequestInfo(const RequestInfo& other);
    ~RequestInfo();

    // Android ID of the device.
    uint64_t android_id;
    // Security token of the device.
    uint64_t security_token;
    // Map of account OAuth2 tokens keyed by emails.
    std::map<std::string, std::string> account_tokens;
    // Digest of GServices settings on the device.
    std::string settings_digest;
    // Information of the Chrome build of this device.
    checkin_proto::ChromeBuildProto chrome_build_proto;
  };

  CheckinRequest(
      const GURL& checkin_url,
      const RequestInfo& request_info,
      const net::BackoffEntry::Policy& backoff_policy,
      const CheckinRequestCallback& callback,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      GCMStatsRecorder* recorder);
  ~CheckinRequest();

  void Start();

  // Invoked from SimpleURLLoader.
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> body);

 private:
  // Schedules a retry attempt with a backoff.
  void RetryWithBackoff();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  CheckinRequestCallback callback_;

  net::BackoffEntry backoff_entry_;
  GURL checkin_url_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  const RequestInfo request_info_;
  base::TimeTicks request_start_time_;

  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // Recorder that records GCM activities for debugging purpose. Not owned.
  GCMStatsRecorder* recorder_;

  base::WeakPtrFactory<CheckinRequest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CheckinRequest);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_CHECKIN_REQUEST_H_
