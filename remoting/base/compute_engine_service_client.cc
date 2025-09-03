// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/compute_engine_service_client.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/environment.h"
#include "base/strings/stringprintf.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/http_status.h"
#include "remoting/base/logging.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace remoting {

namespace {

// Compute Engine VM Instances always have an HTTP metadata server endpoint.
// Shielded Instances also provide an HTTPS endpoint. For compatibility, we
// use the HTTP endpoint for fail-fast decisions or to provide an identity token
// but rely on our backend services to validate the token and metadata. More
// info at: https://cloud.google.com/compute/docs/metadata/querying-metadata.
constexpr char kDefaultMetadataServerHost[] = "metadata.google.internal";
// TODO: joedow - Add support for the HTTPS endpoint:
// https://cloud.google.com/compute/docs/metadata/querying-metadata#query-https-mds
constexpr char kHttpMetadataServerBaseUrlFormat[] =
    "http://%s/computeMetadata/v1/instance/service-accounts/default";
// Cloud users can configure a custom metadata server for the VM and direct CLI
// tools to call it instead by providing a hostname, hostname:port, or IP
// address in this environment variable.
// https://cloud.google.com/nodejs/docs/reference/gcp-metadata/latest#environment-variables
constexpr char kGceMetadataHostVarName[] = "GCE_METADATA_HOST";

constexpr size_t kMaxResponseSize = 4096;

constexpr net::NetworkTrafficAnnotationTag kInstanceIdentityTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "remoting_compute_engine_instance_identity_token",
        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Retrieves a Compute Engine VM Instance Identity token for use by "
            "Chrome Remote Desktop."
          trigger:
            "The request is made when a Compute Engine VM Instance is "
            "configured for remote acces via Chrome Remote Desktop. It is also "
            "called when the host instance makes a backend service request as "
            "the identity token is used to verify the origin of the request."
          data: "Arbitrary payload data used to prevent replay attacks."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chromoting-team@google.com"
            }
          }
          user_data {
            type: ARBITRARY_DATA
          }
          last_reviewed: "2025-02-08"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request will not be sent if Chrome Remote Desktop is not "
            "used within a Compute Engine VM Instance."
          policy_exception_justification:
            "Not implemented."
        })");

constexpr net::NetworkTrafficAnnotationTag kAccessTokenTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "remoting_compute_engine_instance_access_token",
        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Retrieves an OAuth access token for the default service account "
            "associated with the Compute Engine VM Instance."
          trigger:
            "The request is made when Chrome Remote Desktop is being run in a "
            "Compute Engine Instance and needs to send a request to our API "
            "using the default service account for the Compute Engine Instance."
          data: "None"
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chromoting-team@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2025-02-08"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request will not be sent if Chrome Remote Desktop is not "
            "used within a Compute Engine VM Instance."
          policy_exception_justification:
            "Not implemented."
        })");

constexpr net::NetworkTrafficAnnotationTag kAccessTokenScopesTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "remoting_compute_engine_instance_access_token_scopes",
        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Retrieves the set of OAuth scopes included in access tokens which "
            "are generated for the default service account associated with the "
            "Compute Engine VM Instance."
          trigger:
            "The request is made when Chrome Remote Desktop is being run in a "
            "Compute Engine Instance and needs to determine if the default "
            "service account has been configured properly to access our API."
          data: "None"
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chromoting-team@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2025-02-08"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request will not be sent if Chrome Remote Desktop is not "
            "used within a Compute Engine VM Instance."
          policy_exception_justification:
            "Not implemented."
        })");

}  // namespace

ComputeEngineServiceClient::ComputeEngineServiceClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {
  auto env = base::Environment::Create();
  auto metadata_host_override = env->GetVar(kGceMetadataHostVarName);
  metadata_server_base_url_ = base::StringPrintf(
      kHttpMetadataServerBaseUrlFormat,
      metadata_host_override.has_value() && !metadata_host_override->empty()
          ? *metadata_host_override
          : kDefaultMetadataServerHost);
  HOST_LOG << "Using Metadata server URL: " << metadata_server_base_url_;
}

ComputeEngineServiceClient::~ComputeEngineServiceClient() = default;

void ComputeEngineServiceClient::GetInstanceIdentityToken(
    std::string_view audience,
    ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Use 'format=full' to include project and instance details in the token.
  ExecuteRequest(base::StringPrintf("%s/identity?audience=%s&format=full",
                                    metadata_server_base_url_, audience),
                 kInstanceIdentityTrafficAnnotation, std::move(callback));
}

void ComputeEngineServiceClient::GetServiceAccountAccessToken(
    ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ExecuteRequest(base::StringPrintf("%s/token", metadata_server_base_url_),
                 kAccessTokenTrafficAnnotation, std::move(callback));
}

void ComputeEngineServiceClient::GetServiceAccountScopes(
    ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ExecuteRequest(base::StringPrintf("%s/scopes", metadata_server_base_url_),
                 kAccessTokenScopesTrafficAnnotation, std::move(callback));
}

void ComputeEngineServiceClient::CancelPendingRequests() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  url_loader_.reset();
}

void ComputeEngineServiceClient::ExecuteRequest(
    std::string_view url,
    const net::NetworkTrafficAnnotationTag& network_annotation,
    ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO: joedow - Update to handle concurrent requests when needed.
  CHECK(!url_loader_);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = net::HttpRequestHeaders::kGetMethod;
  // All Compute Engine Metadata requests must set this header.
  resource_request->headers.SetHeader("Metadata-Flavor", "Google");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 network_annotation);
  url_loader_->SetTimeoutDuration(base::Seconds(60));
  url_loader_->SetAllowHttpErrorResults(false);
  url_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_5XX |
             network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ComputeEngineServiceClient::OnRequestComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      kMaxResponseSize);
}

void ComputeEngineServiceClient::OnRequestComplete(
    ResponseCallback callback,
    std::optional<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  net::Error net_error = static_cast<net::Error>(url_loader_->NetError());
  HttpStatus http_status = HttpStatus::OK();
  if (net_error != net::Error::OK &&
      net_error != net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE) {
    http_status = HttpStatus(net_error);
  } else if (!url_loader_->ResponseInfo() ||
             !url_loader_->ResponseInfo()->headers ||
             url_loader_->ResponseInfo()->headers->response_code() <= 0) {
    http_status =
        HttpStatus(HttpStatus::Code::INTERNAL,
                   "Failed to get HTTP status from the response header.");
  } else {
    http_status =
        HttpStatus(static_cast<net::HttpStatusCode>(
                       url_loader_->ResponseInfo()->headers->response_code()),
                   response_body.value_or(std::string()));
  }

  if (!http_status.ok()) {
    LOG(ERROR) << "Compute Engine API request failed. Code: "
               << static_cast<int32_t>(http_status.error_code())
               << ", Message: " << http_status.error_message();
  }

  // Reset |url_loader_| since we've extracted the info we need from it.
  // This will allow the caller to reuse this instance to make another request.
  url_loader_.reset();

  std::move(callback).Run(http_status);
}

}  // namespace remoting
