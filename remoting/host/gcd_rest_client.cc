// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gcd_rest_client.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "remoting/base/logging.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace remoting {

GcdRestClient::GcdRestClient(
    const std::string& gcd_base_url,
    const std::string& gcd_device_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuthTokenGetter* token_getter)
    : gcd_base_url_(gcd_base_url),
      gcd_device_id_(gcd_device_id),
      url_loader_factory_(url_loader_factory),
      token_getter_(token_getter),
      clock_(base::DefaultClock::GetInstance()) {}

GcdRestClient::~GcdRestClient() = default;

void GcdRestClient::PatchState(
    std::unique_ptr<base::DictionaryValue> patch_details,
    const GcdRestClient::ResultCallback& callback) {
  DCHECK(!HasPendingRequest());

  // Construct a status update message in the format GCD expects.  The
  // message looks like this, where "..." is filled in from
  // |patch_details|:
  //
  // {
  //   requestTimeMs: T,
  //   patches: [{
  //     timeMs: T,
  //     patch: {...}
  //   }]
  // }
  //
  // Note that |now| is deliberately using a double to hold an integer
  // value because |DictionaryValue| doesn't support int64_t values, and
  // GCD doesn't accept fractional values.
  double now = clock_->Now().ToJavaTime();
  std::unique_ptr<base::DictionaryValue> patch_dict(new base::DictionaryValue);
  patch_dict->SetDouble("requestTimeMs", now);
  std::unique_ptr<base::ListValue> patch_list(new base::ListValue);
  std::unique_ptr<base::DictionaryValue> patch_item(new base::DictionaryValue);
  patch_item->Set("patch", std::move(patch_details));
  patch_item->SetDouble("timeMs", now);
  patch_list->Append(std::move(patch_item));
  patch_dict->Set("patches", std::move(patch_list));

  // Stringify the message.
  if (!base::JSONWriter::Write(*patch_dict, &patch_string_)) {
    LOG(ERROR) << "Error building GCD device state patch.";
    callback.Run(OTHER_ERROR);
    return;
  }
  DLOG(INFO) << "sending state patch: " << patch_string_;

  std::string url =
      gcd_base_url_ + "/devices/" + gcd_device_id_ + "/patchState";

  // Prepare an HTTP request to issue once an auth token is available.
  callback_ = callback;

  resource_request_ = std::make_unique<network::ResourceRequest>();
  resource_request_->url = GURL(url);
  resource_request_->method = "POST";

  token_getter_->CallWithToken(
      base::BindOnce(&GcdRestClient::OnTokenReceived, base::Unretained(this)));
}

void GcdRestClient::SetClockForTest(base::Clock* clock) {
  clock_ = clock;
}

void GcdRestClient::OnTokenReceived(OAuthTokenGetter::Status status,
                                    const std::string& user_email,
                                    const std::string& access_token) {
  DCHECK(HasPendingRequest());

  if (status != OAuthTokenGetter::SUCCESS) {
    LOG(ERROR) << "Error getting OAuth token for GCD request: "
               << resource_request_->url;
    if (status == OAuthTokenGetter::NETWORK_ERROR) {
      FinishCurrentRequest(NETWORK_ERROR);
    } else {
      FinishCurrentRequest(OTHER_ERROR);
    }
    return;
  }

  resource_request_->headers.SetHeader("Authorization",
                                       std::string("Bearer ") + access_token);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gcd_rest_client",
                                          R"(
        semantics {
          sender: "Gcd Rest Client"
          description: "Alternative signaling mechanism for Chrome Remote "
            "Desktop."
          trigger:
            "The GCD code was added for an investigation about alternative "
            "signaling mechanisms, but is not being used in production."
          data: "No user data."
          destination: OTHER
          destination_other:
            "The Chrome Remote Desktop client/host the user is connecting to."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if user does not use Chrome Remote Desktop."
          policy_exception_justification:
            "Not implemented."
        })");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request_),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(patch_string_, "application/json");

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&GcdRestClient::OnURLLoadComplete,
                     base::Unretained(this)));
}

void GcdRestClient::FinishCurrentRequest(Result result) {
  DCHECK(HasPendingRequest());
  resource_request_.reset();
  url_loader_.reset();
  std::move(callback_).Run(result);
}

void GcdRestClient::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK(HasPendingRequest());

  const GURL& request_url = url_loader_->GetFinalURL();

  Result status = !!response_body ? SUCCESS : OTHER_ERROR;

  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  if (status == SUCCESS) {
    DCHECK(response_code == -1 ||
           (response_code >= 200 && response_code < 300));
    DLOG(INFO) << "GCD request succeeded:" << request_url;
  } else if (response_code == 404) {
    LOG(WARNING) << "Host not found (" << response_code
                 << ") loading URL: " << request_url;
    status = NO_SUCH_HOST;
  } else if (response_code == -1) {
    LOG(ERROR) << "Network error (" << response_code
               << ") loading URL: " << request_url;
    status = NETWORK_ERROR;
  } else {
    LOG(ERROR) << "Error (" << response_code
               << ") loading URL: " << request_url;
  }

  FinishCurrentRequest(status);
}

}  // namespace remoting
