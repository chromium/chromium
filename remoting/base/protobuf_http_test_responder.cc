// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_test_responder.h"

#include "base/containers/adapters.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "net/http/http_status_code.h"
#include "remoting/base/protobuf_http_client_messages.pb.h"
#include "remoting/base/protobuf_http_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace remoting {

namespace {

protobufhttpclient::Status ToProtobufStatus(const ProtobufHttpStatus& status) {
  protobufhttpclient::Status result;
  result.set_code(static_cast<int>(status.error_code()));
  result.set_message(status.error_message());
  return result;
}

}  // namespace

ProtobufHttpTestResponder::ProtobufHttpTestResponder() = default;

ProtobufHttpTestResponder::~ProtobufHttpTestResponder() = default;

// static
bool ProtobufHttpTestResponder::ParseRequestMessage(
    const network::ResourceRequest& resource_request,
    google::protobuf::MessageLite* out_message) {
  std::string unified_data;
  for (const auto& data_element : *resource_request.request_body->elements()) {
    if (data_element.type() == network::DataElement::Tag::kBytes) {
      const auto piece =
          data_element.As<network::DataElementBytes>().AsStringPiece();
      unified_data.append(piece);
    }
  }
  return out_message->ParseFromString(unified_data);
}

scoped_refptr<network::SharedURLLoaderFactory>
ProtobufHttpTestResponder::GetUrlLoaderFactory() {
  return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
      &test_url_loader_factory_);
}

void ProtobufHttpTestResponder::AddResponse(
    const std::string& url,
    const google::protobuf::MessageLite& response_message) {
  test_url_loader_factory_.AddResponse(url,
                                       response_message.SerializeAsString());
}

void ProtobufHttpTestResponder::AddResponseToMostRecentRequestUrl(
    const google::protobuf::MessageLite& response_message) {
  AddResponse(GetMostRecentRequestUrl(), response_message);
}

void ProtobufHttpTestResponder::AddError(
    const std::string& url,
    const ProtobufHttpStatus& error_status) {
  test_url_loader_factory_.AddResponse(
      url, ToProtobufStatus(error_status).SerializeAsString(),
      net::HTTP_INTERNAL_SERVER_ERROR);
}

void ProtobufHttpTestResponder::AddErrorToMostRecentRequestUrl(
    const ProtobufHttpStatus& error_status) {
  AddError(GetMostRecentRequestUrl(), error_status);
}

void ProtobufHttpTestResponder::AddStreamResponse(
    const std::string& url,
    const std::vector<const google::protobuf::MessageLite*>& messages,
    const ProtobufHttpStatus& status) {
  protobufhttpclient::StreamBody messages_body;
  for (const auto* message : messages) {
    messages_body.add_messages(message->SerializeAsString());
  }
  std::string stream_data = messages_body.SerializeAsString();
  protobufhttpclient::StreamBody status_body;
  *status_body.mutable_status() = ToProtobufStatus(status);
  stream_data += status_body.SerializeAsString();
  test_url_loader_factory_.AddResponse(url, stream_data);
}

void ProtobufHttpTestResponder::AddStreamResponseToMostRecentRequestUrl(
    const std::vector<const google::protobuf::MessageLite*>& messages,
    const ProtobufHttpStatus& status) {
  AddStreamResponse(GetMostRecentRequestUrl(), messages, status);
}

bool ProtobufHttpTestResponder::GetRequestMessage(
    const std::string& url,
    google::protobuf::MessageLite* out_message) {
  base::RunLoop().RunUntilIdle();
  auto pending_request_it = base::ranges::find(
      base::Reversed(*test_url_loader_factory_.pending_requests()), url,
      [](const network::TestURLLoaderFactory::PendingRequest& request) {
        return request.request.url.spec();
      });
  if (pending_request_it ==
      test_url_loader_factory_.pending_requests()->rend()) {
    return false;
  }
  return ParseRequestMessage(pending_request_it->request, out_message);
}

bool ProtobufHttpTestResponder::GetMostRecentRequestMessage(
    google::protobuf::MessageLite* out_message) {
  return ParseRequestMessage(GetMostRecentPendingRequest().request,
                             out_message);
}

int ProtobufHttpTestResponder::GetNumPending() {
  base::RunLoop().RunUntilIdle();
  return test_url_loader_factory_.pending_requests()->size();
}

network::TestURLLoaderFactory::PendingRequest&
ProtobufHttpTestResponder::GetPendingRequest(size_t index) {
  base::RunLoop().RunUntilIdle();
  DCHECK_LT(index, test_url_loader_factory_.pending_requests()->size());
  return (*test_url_loader_factory_.pending_requests())[index];
}

network::TestURLLoaderFactory::PendingRequest&
ProtobufHttpTestResponder::GetMostRecentPendingRequest() {
  base::RunLoop().RunUntilIdle();
  DCHECK(!test_url_loader_factory_.pending_requests()->empty());
  return test_url_loader_factory_.pending_requests()->back();
}

std::string ProtobufHttpTestResponder::GetMostRecentRequestUrl() {
  return GetMostRecentPendingRequest().request.url.spec();
}

ProtobufHttpTestResponder::MockInterceptor&
ProtobufHttpTestResponder::GetMockInterceptor() {
  if (!mock_interceptor_) {
    mock_interceptor_ = std::make_unique<MockInterceptor>();
    test_url_loader_factory_.SetInterceptor(mock_interceptor_->Get());
  }
  return *mock_interceptor_;
}

}  // namespace remoting
