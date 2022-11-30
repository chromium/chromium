// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_TEST_RESPONDER_H_
#define REMOTING_BASE_PROTOBUF_HTTP_TEST_RESPONDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace remoting {

class ProtobufHttpStatus;

// Helper class to send responses to Protobuf HTTP requests.
class ProtobufHttpTestResponder final {
 public:
  using MockInterceptor =
      base::MockCallback<network::TestURLLoaderFactory::Interceptor>;

  ProtobufHttpTestResponder();
  ~ProtobufHttpTestResponder();

  ProtobufHttpTestResponder(const ProtobufHttpTestResponder&) = delete;
  ProtobufHttpTestResponder& operator=(const ProtobufHttpTestResponder&) =
      delete;

  static bool ParseRequestMessage(
      const network::ResourceRequest& resource_request,
      google::protobuf::MessageLite* out_message);

  // Returns the URL loader factory to be used to create the ProtobufHttpClient.
  // Note that the returned factory *can't be used* after |this| is deleted.
  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory();

  // Note that if you have multiple requests with the same URL, all of them will
  // be resolved with the same response/error.
  void AddResponse(const std::string& url,
                   const google::protobuf::MessageLite& response_message);
  void AddResponseToMostRecentRequestUrl(
      const google::protobuf::MessageLite& response_message);
  void AddError(const std::string& url, const ProtobufHttpStatus& error_status);
  void AddErrorToMostRecentRequestUrl(const ProtobufHttpStatus& error_status);

  // Adds response to a pending stream then immediately closes it with |status|.
  void AddStreamResponse(
      const std::string& url,
      const std::vector<const google::protobuf::MessageLite*>& messages,
      const ProtobufHttpStatus& status);
  void AddStreamResponseToMostRecentRequestUrl(
      const std::vector<const google::protobuf::MessageLite*>& messages,
      const ProtobufHttpStatus& status);

  // Gets the most recent request message matching the URL and writes it to
  // |out_message|. Returns true if the request message is successfully
  // retrieved.
  bool GetRequestMessage(const std::string& url,
                         google::protobuf::MessageLite* out_message);
  bool GetMostRecentRequestMessage(google::protobuf::MessageLite* out_message);

  // Gets number of pending requests. Unlike
  // network::TestURLLoaderFactory::NumPending(), this method also counts
  // pending but cancelled requests.
  int GetNumPending();

  // Returns the PendingRequest instance available at the given index |index|
  // (including cancelled requests) or null if not existing.
  network::TestURLLoaderFactory::PendingRequest& GetPendingRequest(
      size_t index);

  network::TestURLLoaderFactory::PendingRequest& GetMostRecentPendingRequest();

  std::string GetMostRecentRequestUrl();

  // Installs (if called for the first time) and returns a mock interceptor
  // callback that you can add expectations:
  //
  //   EXPECT_CALL(test_responder_.GetMockInterceptor(), Run(_))
  //       .WillOnce([](const ResourceRequest& request) {...});
  //
  // Note that if you call this without passing it to EXPECT_CALL() or ON_CALL()
  // then the test will fail immediately as long as any request is sent.
  MockInterceptor& GetMockInterceptor();

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<MockInterceptor> mock_interceptor_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_TEST_RESPONDER_H_
