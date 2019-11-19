// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "remoting/ios/facade/host_list_service.h"

#import <Foundation/Foundation.h>

#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/http/http_status_code.h"
#include "remoting/base/grpc_support/grpc_async_executor.h"
#include "remoting/base/grpc_test_support/grpc_async_test_server.h"
#include "remoting/ios/facade/directory_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#define EXPECT_HOST_LIST_STATE(expected) \
  EXPECT_EQ(expected, host_list_service_.state())

#define EXPECT_NO_FETCH_FAILURE() \
  EXPECT_TRUE(host_list_service_.last_fetch_failure() == nullptr)

namespace remoting {

namespace {

using DirectoryService = apis::v1::RemotingDirectoryService;

apis::v1::HostInfo CreateFakeHost(const std::string& host_id) {
  apis::v1::HostInfo fake_host;
  fake_host.set_host_id(host_id);
  return fake_host;
}

apis::v1::GetHostListResponse CreateFakeHostListResponse(
    const std::string& host_id = "fake_host") {
  apis::v1::GetHostListResponse response;
  *response.add_hosts() = CreateFakeHost(host_id);
  return response;
}

}  // namespace

class HostListServiceTest : public PlatformTest {
 public:
  HostListServiceTest();
  ~HostListServiceTest() override;

 protected:
  // Respond to a GetHostList request and block until the host list state is
  // changed.
  void BlockAndRespondGetHostList(
      const grpc::Status& status,
      const apis::v1::GetHostListResponse& response);
  void RespondGetHostList(const grpc::Status& status,
                          const apis::v1::GetHostListResponse& response);
  void NotifyUserUpdate(bool is_signed_in);

  test::GrpcAsyncTestServer fake_directory_server_;
  HostListService host_list_service_;

  int on_fetch_failed_call_count_ = 0;
  int on_host_list_state_changed_call_count_ = 0;

  id remoting_authentication_mock_;
  id remoting_service_mock_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<HostListService::CallbackSubscription>
      host_list_state_subscription_;
  std::unique_ptr<HostListService::CallbackSubscription>
      fetch_failure_subscription_;
};

HostListServiceTest::HostListServiceTest()
    : fake_directory_server_(
          std::make_unique<DirectoryService::AsyncService>()),
      host_list_service_(std::make_unique<DirectoryClient>(
          std::make_unique<GrpcAsyncExecutor>(),
          fake_directory_server_.CreateInProcessChannel())) {
  static const char use_cocoa_locale[] = "";

  l10n_util::OverrideLocaleWithCocoaLocale();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      use_cocoa_locale, NULL, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

  on_fetch_failed_call_count_ = 0;
  on_host_list_state_changed_call_count_ = 0;

  remoting_authentication_mock_ =
      OCMProtocolMock(@protocol(RemotingAuthentication));

  remoting_service_mock_ = OCMClassMock([RemotingService class]);
  OCMStub([remoting_service_mock_ instance]).andReturn(remoting_service_mock_);
  OCMStub([remoting_service_mock_ authentication])
      .andReturn(remoting_authentication_mock_);

  host_list_state_subscription_ =
      host_list_service_.RegisterHostListStateCallback(base::BindRepeating(
          [](HostListServiceTest* that) {
            that->on_host_list_state_changed_call_count_++;
          },
          base::Unretained(this)));
  fetch_failure_subscription_ =
      host_list_service_.RegisterFetchFailureCallback(base::BindRepeating(
          [](HostListServiceTest* that) {
            that->on_fetch_failed_call_count_++;
          },
          base::Unretained(this)));
}

HostListServiceTest::~HostListServiceTest() {
  ui::ResourceBundle::CleanupSharedInstance();
}

void HostListServiceTest::BlockAndRespondGetHostList(
    const grpc::Status& status,
    const apis::v1::GetHostListResponse& response) {
  base::RunLoop run_loop;
  auto subscription =
      host_list_service_.RegisterHostListStateCallback(run_loop.QuitClosure());
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&HostListServiceTest::RespondGetHostList,
                                base::Unretained(this), status, response));
  run_loop.Run();
}

void HostListServiceTest::RespondGetHostList(
    const grpc::Status& status,
    const apis::v1::GetHostListResponse& response) {
  apis::v1::GetHostListRequest request;
  fake_directory_server_
      .HandleRequest(&DirectoryService::AsyncService::RequestGetHostList,
                     &request)
      ->Respond(response, status);
}

void HostListServiceTest::NotifyUserUpdate(bool is_signed_in) {
  NSDictionary* user_info =
      is_signed_in ? @{kUserInfo : [[UserInfo alloc] init]} : @{};
  [NSNotificationCenter.defaultCenter postNotificationName:kUserDidUpdate
                                                    object:nil
                                                  userInfo:user_info];
}

TEST_F(HostListServiceTest, SuccessfullyFetchedOneHost) {
  EXPECT_HOST_LIST_STATE(HostListService::State::NOT_FETCHED);
  EXPECT_NO_FETCH_FAILURE();

  host_list_service_.RequestFetch();
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHING);
  EXPECT_NO_FETCH_FAILURE();

  BlockAndRespondGetHostList(grpc::Status::OK,
                             CreateFakeHostListResponse("fake_host_1"));
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHED);
  EXPECT_NO_FETCH_FAILURE();

  EXPECT_EQ(1u, host_list_service_.hosts().size());
  EXPECT_EQ("fake_host_1", host_list_service_.hosts()[0].host_id());

  EXPECT_EQ(2, on_host_list_state_changed_call_count_);
  EXPECT_EQ(0, on_fetch_failed_call_count_);
}

TEST_F(HostListServiceTest, SuccessfullyFetchedTwoHosts_HostListSorted) {
  EXPECT_HOST_LIST_STATE(HostListService::State::NOT_FETCHED);
  EXPECT_NO_FETCH_FAILURE();

  host_list_service_.RequestFetch();
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHING);
  EXPECT_NO_FETCH_FAILURE();

  apis::v1::GetHostListResponse response;
  response.add_hosts()->set_host_name("Host 2");
  response.add_hosts()->set_host_name("Host 1");
  BlockAndRespondGetHostList(grpc::Status::OK, response);
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHED);
  EXPECT_NO_FETCH_FAILURE();

  EXPECT_EQ(2u, host_list_service_.hosts().size());
  EXPECT_EQ("Host 1", host_list_service_.hosts()[0].host_name());
  EXPECT_EQ("Host 2", host_list_service_.hosts()[1].host_name());

  EXPECT_EQ(2, on_host_list_state_changed_call_count_);
  EXPECT_EQ(0, on_fetch_failed_call_count_);
}

TEST_F(HostListServiceTest, FetchHostListRequestFailed) {
  EXPECT_HOST_LIST_STATE(HostListService::State::NOT_FETCHED);
  EXPECT_NO_FETCH_FAILURE();

  host_list_service_.RequestFetch();
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHING);
  EXPECT_NO_FETCH_FAILURE();

  BlockAndRespondGetHostList(
      grpc::Status(grpc::StatusCode::INTERNAL, "Internal error"), {});
  EXPECT_HOST_LIST_STATE(HostListService::State::NOT_FETCHED);
  EXPECT_EQ(HostListService::FetchFailureReason::UNKNOWN_ERROR,
            host_list_service_.last_fetch_failure()->reason);
  EXPECT_EQ("Internal error",
            host_list_service_.last_fetch_failure()->localized_description);
  EXPECT_TRUE(host_list_service_.hosts().empty());

  EXPECT_EQ(2, on_host_list_state_changed_call_count_);
  EXPECT_EQ(1, on_fetch_failed_call_count_);
}

TEST_F(HostListServiceTest, FetchHostListRequestUnauthenticated_signOut) {
  EXPECT_HOST_LIST_STATE(HostListService::State::NOT_FETCHED);
  EXPECT_NO_FETCH_FAILURE();

  host_list_service_.RequestFetch();
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHING);
  EXPECT_NO_FETCH_FAILURE();

  OCMExpect([remoting_authentication_mock_ logout]);
  BlockAndRespondGetHostList(
      grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Unauthenticated"), {});
  EXPECT_HOST_LIST_STATE(HostListService::State::NOT_FETCHED);
  [remoting_authentication_mock_ verifyAtLocation:nil];

  EXPECT_EQ(2, on_host_list_state_changed_call_count_);
  EXPECT_EQ(1, on_fetch_failed_call_count_);
}

TEST_F(HostListServiceTest, RequestFetchWhileFetching_ignoreSecondRequest) {
  EXPECT_HOST_LIST_STATE(HostListService::State::NOT_FETCHED);

  host_list_service_.RequestFetch();
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHING);

  host_list_service_.RequestFetch();
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHING);

  EXPECT_EQ(1, on_host_list_state_changed_call_count_);
  EXPECT_EQ(0, on_fetch_failed_call_count_);
}

TEST_F(HostListServiceTest, UserLogOut_cancelFetch) {
  EXPECT_HOST_LIST_STATE(HostListService::State::NOT_FETCHED);

  host_list_service_.RequestFetch();
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHING);

  NotifyUserUpdate(false);
  EXPECT_HOST_LIST_STATE(HostListService::State::NOT_FETCHED);

  EXPECT_EQ(2, on_host_list_state_changed_call_count_);
  EXPECT_EQ(0, on_fetch_failed_call_count_);
}

TEST_F(HostListServiceTest, UserSwitchAccount_cancelThenRequestNewFetch) {
  EXPECT_HOST_LIST_STATE(HostListService::State::NOT_FETCHED);

  host_list_service_.RequestFetch();
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHING);

  NotifyUserUpdate(true);

  // This response is for the first user, which will be ignored.
  RespondGetHostList(grpc::Status::OK,
                     CreateFakeHostListResponse("fake_host_id_1"));
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHING);

  // This is for the current user.
  BlockAndRespondGetHostList(grpc::Status::OK,
                             CreateFakeHostListResponse("fake_host_id_2"));
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHED);

  EXPECT_EQ(1u, host_list_service_.hosts().size());
  EXPECT_EQ("fake_host_id_2", host_list_service_.hosts()[0].host_id());

  // Note that there is an extra FETCHING->NOT_FETCH change during
  // NotifyUserUpdate(true).
  EXPECT_EQ(4, on_host_list_state_changed_call_count_);
  EXPECT_EQ(0, on_fetch_failed_call_count_);
}

}  // namespace remoting
