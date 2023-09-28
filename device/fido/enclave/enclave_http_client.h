// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ENCLAVE_HTTP_CLIENT_H_
#define DEVICE_FIDO_ENCLAVE_ENCLAVE_HTTP_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "net/url_request/url_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace net {
class IOBuffer;
}

namespace device::enclave {

class EnclaveHttpClient : public net::URLRequest::Delegate {
 public:
  using RequestCallback =
      base::RepeatingCallback<void(int, absl::optional<std::vector<uint8_t>>)>;

  enum class RequestType {
    kNone,
    kInit,
    kCommand,
  };

  EnclaveHttpClient(const GURL& service_url,
                    const std::string& username,
                    RequestCallback on_request_done);
  ~EnclaveHttpClient() override;

  EnclaveHttpClient(const EnclaveHttpClient&) = delete;
  EnclaveHttpClient& operator=(const EnclaveHttpClient&) = delete;

  // Sends an HTTP request to the service, with |type| determining the path,
  // and |data| included in the HTTP body. Invokes |on_request_done| when
  // the request has completed, providing the success status and the response
  // body.
  void SendHttpRequest(RequestType type, base::span<const uint8_t> data);

 private:
  // net::URLRequest::Delegate:
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  // Encodes the payloads and constructs a JSON string in |post_body_|;
  void BuildInitBody(base::span<const uint8_t> data);
  void BuildCommandBody(base::span<const uint8_t> data);

  // Decodes the body JSON from the HTTP responses.
  absl::optional<std::vector<uint8_t>> ParseInitResponse(
      const base::Value::Dict& response_dict);
  absl::optional<std::vector<uint8_t>> ParseCommandResponse(
      const base::Value::Dict& response_dict);

  void Read(net::URLRequest* request);
  bool ConsumeBytesRead(net::URLRequest* request, int num_bytes);

  void CompleteRequest(int bytes_received);

  RequestType request_in_progress_ = RequestType::kNone;

  GURL service_url_;
  std::string username_;
  RequestCallback on_request_done_;

  // url_request_ has to be before url_request_context_ for destruction
  // ordering.
  std::unique_ptr<net::URLRequest> url_request_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  std::vector<char> response_body_;
  absl::optional<std::string> post_body_;

  std::string session_id_;
};

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_ENCLAVE_HTTP_CLIENT_H_
