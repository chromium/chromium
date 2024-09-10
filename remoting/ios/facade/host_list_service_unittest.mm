// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/facade/host_list_service.h"

#import <Foundation/Foundation.h>

#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/http/http_status_code.h"
#include "remoting/base/directory_service_client.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/protobuf_http_test_responder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"

#define EXPECT_HOST_LIST_STATE(expected) \
  EXPECT_EQ(expected, host_list_service_.state())

#define EXPECT_NO_FETCH_FAILURE() \
  EXPECT_TRUE(host_list_service_.last_fetch_failure() == nullptr)

namespace remoting {

namespace {

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
      const ProtobufHttpStatus& status,
      const apis::v1::GetHostListResponse& response);
  void NotifyUserUpdate(bool is_signed_in);

  base::test::TaskEnvironment task_environment_;
  ProtobufHttpTestResponder test_responder_;
  FakeOAuthTokenGetter oauth_token_getter_{OAuthTokenGetter::Status::SUCCESS,
                                           "", "", ""};
  HostListService host_list_service_;

  int on_fetch_failed_call_count_ = 0;
  int on_host_list_state_changed_call_count_ = 0;

  id remoting_authentication_mock_;
  id remoting_service_mock_;

 private:
  base::CallbackListSubscription host_list_state_subscription_;
  base::CallbackListSubscription fetch_failure_subscription_;
  display::ScopedNativeScreen screen_;
};

HostListServiceTest::HostListServiceTest()
    : host_list_service_(base::SequenceBound<DirectoryServiceClient>(
          task_environment_.GetMainThreadTaskRunner(),
          &oauth_token_getter_,
          test_responder_.GetUrlLoaderFactory())) {
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
    const ProtobufHttpStatus& status,
    const apis::v1::GetHostListResponse& response) {
  base::RunLoop run_loop;
  auto subscription =
      host_list_service_.RegisterHostListStateCallback(run_loop.QuitClosure());
  if (status.ok()) {
    test_responder_.AddResponseToMostRecentRequestUrl(response);
  } else {
    test_responder_.AddErrorToMostRecentRequestUrl(status);
  }
  run_loop.Run();
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

  BlockAndRespondGetHostList(ProtobufHttpStatus::OK(),
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
  BlockAndRespondGetHostList(ProtobufHttpStatus::OK(), response);
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
      ProtobufHttpStatus(ProtobufHttpStatus::Code::INTERNAL, "Internal error"),
      {});
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
      ProtobufHttpStatus(ProtobufHttpStatus::Code::UNAUTHENTICATED,
                         "Unauthenticated"),
      {});
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
  ASSERT_EQ(1, test_responder_.GetNumPending());

  NotifyUserUpdate(true);

  // The HostListService should have cancelled the previous request and created
  // a new request.
  ASSERT_EQ(2, test_responder_.GetNumPending());
  ASSERT_FALSE(test_responder_.GetPendingRequest(0).client.is_connected());
  ASSERT_TRUE(test_responder_.GetPendingRequest(1).client.is_connected());
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHING);

  BlockAndRespondGetHostList(ProtobufHttpStatus::OK(),
                             CreateFakeHostListResponse("fake_host_id"));
  EXPECT_HOST_LIST_STATE(HostListService::State::FETCHED);

  EXPECT_EQ(1u, host_list_service_.hosts().size());
  EXPECT_EQ("fake_host_id", host_list_service_.hosts()[0].host_id());

  // Note that there is an extra FETCHING->NOT_FETCH change during
  // NotifyUserUpdate(true).
  EXPECT_EQ(4, on_host_list_state_changed_call_count_);
  EXPECT_EQ(0, on_fetch_failed_call_count_);
}

}  // namespace remoting
