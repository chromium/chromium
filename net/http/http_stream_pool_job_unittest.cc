// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "net/base/load_states.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_factory_test_util.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_group.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace net {

using Group = HttpStreamPool::Group;
using Job = HttpStreamPool::Job;

namespace {

class FakeServiceEndpointRequest : public HostResolver::ServiceEndpointRequest {
 public:
  FakeServiceEndpointRequest() = default;

  void set_start_result(int start_result) { start_result_ = start_result; }

  void set_endpoints(std::vector<ServiceEndpoint> endpoints) {
    endpoints_ = std::move(endpoints);
  }

  void set_aliases(std::set<std::string> aliases) {
    aliases_ = std::move(aliases);
  }

  void set_endpoints_crypto_ready(bool endpoints_crypto_ready) {
    endpoints_crypto_ready_ = endpoints_crypto_ready;
  }

  void set_resolve_error_info(ResolveErrorInfo resolve_error_info) {
    resolve_error_info_ = resolve_error_info;
  }

  RequestPriority priority() const { return priority_; }
  void set_priority(RequestPriority priority) { priority_ = priority; }

  void CallOnServiceEndpointsUpdated();

  void CallOnServiceEndpointRequestFinished(int rv);

  // HostResolver::ServiceEndpointRequest methods:
  int Start(Delegate* delegate) override;
  const std::vector<ServiceEndpoint>& GetEndpointResults() override;
  const std::set<std::string>& GetDnsAliasResults() override;
  bool EndpointsCryptoReady() override;
  ResolveErrorInfo GetResolveErrorInfo() override;
  void ChangeRequestPriority(RequestPriority priority) override;

 private:
  raw_ptr<Delegate> delegate_;

  int start_result_ = ERR_IO_PENDING;
  std::vector<ServiceEndpoint> endpoints_;
  std::set<std::string> aliases_;
  bool endpoints_crypto_ready_ = true;
  ResolveErrorInfo resolve_error_info_;
  RequestPriority priority_ = RequestPriority::IDLE;
};

void FakeServiceEndpointRequest::CallOnServiceEndpointsUpdated() {
  CHECK(delegate_);
  delegate_->OnServiceEndpointsUpdated();
}

void FakeServiceEndpointRequest::CallOnServiceEndpointRequestFinished(int rv) {
  CHECK(delegate_);
  delegate_->OnServiceEndpointRequestFinished(rv);
}

int FakeServiceEndpointRequest::Start(Delegate* delegate) {
  CHECK(!delegate_);
  CHECK(delegate);
  delegate_ = delegate;
  return start_result_;
}

const std::vector<ServiceEndpoint>&
FakeServiceEndpointRequest::GetEndpointResults() {
  return endpoints_;
}

const std::set<std::string>& FakeServiceEndpointRequest::GetDnsAliasResults() {
  return aliases_;
}

bool FakeServiceEndpointRequest::EndpointsCryptoReady() {
  return endpoints_crypto_ready_;
}

ResolveErrorInfo FakeServiceEndpointRequest::GetResolveErrorInfo() {
  return resolve_error_info_;
}

void FakeServiceEndpointRequest::ChangeRequestPriority(
    RequestPriority priority) {
  priority_ = priority;
}

class FakeServiceEndpointResolver : public HostResolver {
 public:
  FakeServiceEndpointResolver() = default;

  FakeServiceEndpointResolver(const FakeServiceEndpointResolver&) = delete;
  FakeServiceEndpointResolver& operator=(const FakeServiceEndpointResolver&) =
      delete;

  ~FakeServiceEndpointResolver() override = default;

  FakeServiceEndpointRequest* AddFakeRequest();

  // HostResolver methods:
  void OnShutdown() override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      url::SchemeHostPort host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      std::optional<ResolveHostParameters> optional_parameters) override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetworkAnonymizationKey& network_anonymization_key,
      const NetLogWithSource& net_log,
      const std::optional<ResolveHostParameters>& optional_parameters) override;
  std::unique_ptr<ServiceEndpointRequest> CreateServiceEndpointRequest(
      Host host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      ResolveHostParameters parameters) override;

 private:
  std::list<std::unique_ptr<FakeServiceEndpointRequest>> requests_;
};

FakeServiceEndpointRequest* FakeServiceEndpointResolver::AddFakeRequest() {
  std::unique_ptr<FakeServiceEndpointRequest> request =
      std::make_unique<FakeServiceEndpointRequest>();
  FakeServiceEndpointRequest* raw_request = request.get();
  requests_.emplace_back(std::move(request));
  return raw_request;
}

void FakeServiceEndpointResolver::OnShutdown() {}

std::unique_ptr<HostResolver::ResolveHostRequest>
FakeServiceEndpointResolver::CreateRequest(
    url::SchemeHostPort host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    std::optional<ResolveHostParameters> optional_parameters) {
  NOTREACHED_NORETURN();
}

std::unique_ptr<HostResolver::ResolveHostRequest>
FakeServiceEndpointResolver::CreateRequest(
    const HostPortPair& host,
    const NetworkAnonymizationKey& network_anonymization_key,
    const NetLogWithSource& net_log,
    const std::optional<ResolveHostParameters>& optional_parameters) {
  NOTREACHED_NORETURN();
}

std::unique_ptr<HostResolver::ServiceEndpointRequest>
FakeServiceEndpointResolver::CreateServiceEndpointRequest(
    Host host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    ResolveHostParameters parameters) {
  CHECK(!requests_.empty());
  std::unique_ptr<FakeServiceEndpointRequest> request =
      std::move(requests_.front());
  requests_.pop_front();
  request->set_priority(parameters.initial_priority);
  return request;
}

class StreamRequester {
 public:
  StreamRequester() : destination_(url::SchemeHostPort("http", "a.test", 80)) {}

  StreamRequester& set_priority(RequestPriority priority) {
    priority_ = priority;
    return *this;
  }

  HttpStreamKey GetStreamKey() const {
    return HttpStreamKey(destination_, privacy_mode_, SocketTag(),
                         NetworkAnonymizationKey(), secure_dns_policy_,
                         disable_cert_network_fetches_);
  }

  std::unique_ptr<HttpStreamRequest> RequestStream(
      HttpStreamPool& pool,
      HttpStreamRequest::Delegate* delegate) {
    HttpStreamKey stream_key = GetStreamKey();
    Group& group = pool.GetOrCreateGroupForTesting(stream_key);
    return group.RequestStream(delegate, priority_, NetLogWithSource());
  }

 private:
  url::SchemeHostPort destination_;
  PrivacyMode privacy_mode_ = PRIVACY_MODE_DISABLED;
  SecureDnsPolicy secure_dns_policy_ = SecureDnsPolicy::kAllow;
  bool disable_cert_network_fetches_ = true;

  RequestPriority priority_ = RequestPriority::IDLE;
};

}  // namespace

class HttpStreamPoolJobTest : public TestWithTaskEnvironment {
 public:
  HttpStreamPoolJobTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    session_deps_.alternate_host_resolver =
        std::make_unique<FakeServiceEndpointResolver>();
    http_network_session_ =
        SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    pool_ = std::make_unique<HttpStreamPool>(http_network_session_.get());
  }

 protected:
  HttpStreamPool& pool() { return *pool_; }

  FakeServiceEndpointResolver* resolver() {
    return static_cast<FakeServiceEndpointResolver*>(
        session_deps_.alternate_host_resolver.get());
  }

  MockHttpStreamRequestDelegate& request_delegate() {
    return request_delegate_;
  }

 private:
  MockHttpStreamRequestDelegate request_delegate_;
  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> http_network_session_;
  std::unique_ptr<HttpStreamPool> pool_;
};

TEST_F(HttpStreamPoolJobTest, ResolveEndpointFailedSync) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request->set_start_result(ERR_FAILED);
  EXPECT_CALL(request_delegate(), OnStreamFailed(ERR_FAILED, _, _, _)).Times(1);
  std::unique_ptr<HttpStreamRequest> request =
      StreamRequester().RequestStream(pool(), &request_delegate());
}

TEST_F(HttpStreamPoolJobTest, ResolveEndpointFailedMultipleRequests) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  MockHttpStreamRequestDelegate request_delegate1;
  std::unique_ptr<HttpStreamRequest> request1 =
      StreamRequester().RequestStream(pool(), &request_delegate1);

  MockHttpStreamRequestDelegate request_delegate2;
  std::unique_ptr<HttpStreamRequest> request2 =
      StreamRequester().RequestStream(pool(), &request_delegate2);

  EXPECT_CALL(request_delegate1, OnStreamFailed(ERR_FAILED, _, _, _)).Times(1);
  EXPECT_CALL(request_delegate2, OnStreamFailed(ERR_FAILED, _, _, _)).Times(1);

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_FAILED);
  RunUntilIdle();
}

TEST_F(HttpStreamPoolJobTest, LoadState) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  std::unique_ptr<HttpStreamRequest> request =
      StreamRequester().RequestStream(pool(), &request_delegate());

  ASSERT_EQ(request->GetLoadState(), LOAD_STATE_RESOLVING_HOST);

  EXPECT_CALL(request_delegate(), OnStreamFailed(ERR_FAILED, _, _, _)).Times(1);

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_FAILED);
  RunUntilIdle();
  ASSERT_EQ(request->GetLoadState(), LOAD_STATE_IDLE);
}

TEST_F(HttpStreamPoolJobTest, ResolveErrorInfo) {
  ResolveErrorInfo resolve_error_info(ERR_NAME_NOT_RESOLVED);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request->set_resolve_error_info(resolve_error_info);

  std::unique_ptr<HttpStreamRequest> request =
      StreamRequester().RequestStream(pool(), &request_delegate());

  EXPECT_CALL(request_delegate(),
              OnStreamFailed(ERR_NAME_NOT_RESOLVED, _, _, resolve_error_info))
      .Times(1);

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_NAME_NOT_RESOLVED);
  RunUntilIdle();
}

TEST_F(HttpStreamPoolJobTest, SetPriority) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  MockHttpStreamRequestDelegate request_delegate1;
  std::unique_ptr<HttpStreamRequest> request1 =
      StreamRequester()
          .set_priority(RequestPriority::LOW)
          .RequestStream(pool(), &request_delegate1);
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::LOW);

  MockHttpStreamRequestDelegate request_delegate2;
  std::unique_ptr<HttpStreamRequest> request2 =
      StreamRequester()
          .set_priority(RequestPriority::IDLE)
          .RequestStream(pool(), &request_delegate2);
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::LOW);

  request1->SetPriority(RequestPriority::HIGHEST);
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::HIGHEST);

  // TODO(crbug.com/346835898): Check `request1` completes first. We need to
  // implement the success case for the check.
}

}  // namespace net
