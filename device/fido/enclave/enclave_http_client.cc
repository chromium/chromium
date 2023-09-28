// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_http_client.h"

#include "base/base64url.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_piece.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/enclave/enclave_protocol_utils.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_element_reader.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace device::enclave {
namespace {
constexpr int kReadBufferSize = 2048;

// An arbitrary cap on the HTTP response size.
constexpr int kMaxResponseSize = 1 << 16;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("passkey_enclave_client", R"(
        semantics {
          sender: "Cloud Enclave Passkey Authenticator Client"
          description:
            "Chrome can use a cloud-based authenticator running in a trusted "
            "execution environment to fulfill WebAuthn getAssertion requests "
            "for passkeys synced to Chrome from Google Password Manager. This "
            "is used on desktop platforms where there is not a way to safely "
            "unwrap the private keys with a lock screen knowledge factor. "
            "This traffic creates an encrypted session with the enclave "
            "service and carries the request and response over that session."
          trigger:
            "A web site initiates a WebAuthn request for passkeys on a device "
            "that has been enrolled with the cloud authenticator, and there "
            "is an available Google Password Manager passkey that can be used "
            "to provide the assertion."
          user_data {
            type: PROFILE_DATA
            type: CREDENTIALS
          }
          data: "This contains an encrypted WebAuthn assertion request as "
            "well as an encrypted passkey which can only be unwrapped by the "
            "enclave service."
          internal {
            contacts {
                email: "chrome-webauthn@google.com"
            }
          }
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2023-07-05"
        }
        policy {
          cookies_allowed: NO
          setting: "Users can disable this authenticator by opening settings "
            "and signing out of the Google account in their profile, or by "
            "disabling password sync on the profile. Password sync can be "
            "disabled from the Sync and Google Services screen."
          chrome_policy {
            SyncDisabled {
              SyncDisabled: true
            }
            SyncTypesListDisabled {
              SyncTypesListDisabled: {
                entries: "passwords"
              }
            }
          }
        })");

}  // namespace

EnclaveHttpClient::EnclaveHttpClient(const GURL& service_url,
                                     const std::string& username,
                                     RequestCallback on_request_done)
    : service_url_(service_url),
      username_(username),
      on_request_done_(std::move(on_request_done)) {
  net::URLRequestContextBuilder builder;
  builder.DisableHttpCache();
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation()));
  url_request_context_ = builder.Build();
}

EnclaveHttpClient::~EnclaveHttpClient() {
  url_request_.reset();
}

void EnclaveHttpClient::SendHttpRequest(RequestType type,
                                        base::span<const uint8_t> data) {
  CHECK(!url_request_);
  CHECK(request_in_progress_ == RequestType::kNone);
  request_in_progress_ = type;

  if (!read_buffer_) {
    read_buffer_ = base::MakeRefCounted<net::IOBuffer>(kReadBufferSize);
  }

  GURL request_url;
  GURL::Replacements replacement;

  switch (type) {
    case RequestType::kInit: {
      std::string path = base::StrCat({username_, "/", kInitPath});
      replacement.SetPathStr(path);
      request_url = service_url_.ReplaceComponents(replacement);
      BuildInitBody(data);
      break;
    }
    case RequestType::kCommand: {
      std::string path = base::StrCat({username_, "/", kCommandPath});
      replacement.SetPathStr(path);
      request_url = service_url_.ReplaceComponents(replacement);
      BuildCommandBody(data);
      break;
    }
    case RequestType::kNone:
      NOTREACHED();
  }

  url_request_ = url_request_context_->CreateRequest(
      request_url, net::DEFAULT_PRIORITY, this, kTrafficAnnotation,
      /*is_for_websockets=*/false);
  url_request_->set_method("POST");
  if (post_body_) {
    std::unique_ptr<net::UploadElementReader> reader(
        new net::UploadBytesElementReader(
            base::StringPiece(post_body_.value()).data(),
            post_body_->length()));
    url_request_->set_upload(
        net::ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));
  }
  url_request_->Start();
}

void EnclaveHttpClient::BuildInitBody(base::span<const uint8_t> data) {
  std::string encoded_data;
  base::Base64UrlEncode(data, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_data);
  base::Value::Dict values;
  values.Set(kInitSessionRequestData, encoded_data);
  post_body_ = std::string();
  base::JSONWriter::Write(values, &post_body_.value());
}

void EnclaveHttpClient::BuildCommandBody(base::span<const uint8_t> data) {
  std::string encoded_data;
  base::Base64UrlEncode(data, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_data);
  base::Value::Dict values;
  values.Set(kSessionId, session_id_);
  values.Set(kSendCommandRequestData, encoded_data);
  post_body_ = std::string();
  base::JSONWriter::Write(values, &post_body_.value());
}

void EnclaveHttpClient::OnResponseStarted(net::URLRequest* request,
                                          int net_error) {
  CHECK(request == url_request_.get());

  if (net_error != net::OK) {
    CompleteRequest(net_error);
    return;
  }

  if (request->GetResponseCode() != 200) {
    CompleteRequest(net::ERR_HTTP_RESPONSE_CODE_FAILURE);
    return;
  }

  Read(request);
}

void EnclaveHttpClient::OnReadCompleted(net::URLRequest* request,
                                        int bytes_read) {
  CHECK(request == url_request_.get());

  if (ConsumeBytesRead(request, bytes_read)) {
    Read(request);
  }
}

void EnclaveHttpClient::Read(net::URLRequest* request) {
  int num_bytes = 0;
  while (num_bytes >= 0) {
    num_bytes = request->Read(read_buffer_.get(), kReadBufferSize);
    if (num_bytes == net::ERR_IO_PENDING ||
        !ConsumeBytesRead(request, num_bytes)) {
      return;
    }
  }

  CompleteRequest(net::OK);
}

bool EnclaveHttpClient::ConsumeBytesRead(net::URLRequest* request,
                                         int num_bytes) {
  if (num_bytes == 0) {
    // EOF
    CompleteRequest(net::OK);
    return false;
  }

  if (num_bytes < 0) {
    CompleteRequest(net::ERR_FAILED);
    return false;
  }

  if ((base::CheckAdd(num_bytes, response_body_.size())).ValueOrDie() >
      kMaxResponseSize) {
    CompleteRequest(net::ERR_FILE_TOO_BIG);
    return false;
  }

  // Append the data to the response.
  response_body_.insert(response_body_.end(), read_buffer_->data(),
                        read_buffer_->data() + num_bytes);
  return true;
}

void EnclaveHttpClient::CompleteRequest(int status) {
  url_request_.reset();
  absl::optional<std::vector<uint8_t>> response_data;
  if (status == net::OK) {
    base::StringPiece response_body_string(response_body_.data(),
                                           response_body_.size());
    absl::optional<base::Value> response_dict =
        base::JSONReader::Read(response_body_string);
    if (response_dict && response_dict->is_dict()) {
      switch (request_in_progress_) {
        case RequestType::kInit:
          response_data = ParseInitResponse(response_dict->GetDict());
          break;
        case RequestType::kCommand:
          response_data = ParseCommandResponse(response_dict->GetDict());
          break;
        case RequestType::kNone:
          NOTREACHED();
      }
      if (!response_data) {
        status = net::ERR_INVALID_RESPONSE;
      }
    } else {
      FIDO_LOG(ERROR)
          << "Handshake response from enclave service is not valid JSON.";
      status = net::ERR_INVALID_RESPONSE;
    }
  }
  response_body_.clear();
  request_in_progress_ = RequestType::kNone;
  on_request_done_.Run(status, std::move(response_data));
}

absl::optional<std::vector<uint8_t>> EnclaveHttpClient::ParseInitResponse(
    const base::Value::Dict& response_dict) {
  const std::string* session_id = response_dict.FindString(kSessionId);
  if (!session_id) {
    FIDO_LOG(ERROR)
        << "Handshake response from enclave service missing session ID.";
    return absl::nullopt;
  }
  const std::string* handshake_response_value =
      response_dict.FindString(kInitSessionResponseData);
  if (!handshake_response_value) {
    FIDO_LOG(ERROR) << "Handshake response from enclave service missing data.";
    return absl::nullopt;
  }
  absl::optional<std::vector<uint8_t>> decoded_response = base::Base64UrlDecode(
      *handshake_response_value, base::Base64UrlDecodePolicy::DISALLOW_PADDING);
  if (!decoded_response) {
    FIDO_LOG(ERROR)
        << "Handshake response data from enclave service failed to decode.";
    return absl::nullopt;
  }

  session_id_ = std::move(*session_id);
  return decoded_response;
}

absl::optional<std::vector<uint8_t>> EnclaveHttpClient::ParseCommandResponse(
    const base::Value::Dict& response_dict) {
  const std::string* command_response_value =
      response_dict.FindString(kSendCommandResponseData);
  if (!command_response_value) {
    FIDO_LOG(ERROR) << "Command response from enclave service missing data.";
    return absl::nullopt;
  }
  absl::optional<std::vector<uint8_t>> decoded_response = base::Base64UrlDecode(
      *command_response_value, base::Base64UrlDecodePolicy::DISALLOW_PADDING);
  if (!decoded_response) {
    FIDO_LOG(ERROR)
        << "Command response data from enclave service failed to decode.";
    return absl::nullopt;
  }

  return decoded_response;
}

}  // namespace device::enclave
