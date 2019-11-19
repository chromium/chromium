// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/chromoting_test_driver_environment.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_executor.h"
#include "remoting/test/access_token_fetcher.h"
#include "remoting/test/host_list_fetcher.h"
#include "remoting/test/test_token_storage.h"

namespace remoting {
namespace test {

ChromotingTestDriverEnvironment* g_chromoting_shared_data = nullptr;

ChromotingTestDriverEnvironment::EnvironmentOptions::EnvironmentOptions() =
    default;

ChromotingTestDriverEnvironment::EnvironmentOptions::~EnvironmentOptions() =
    default;

ChromotingTestDriverEnvironment::ChromotingTestDriverEnvironment(
    const EnvironmentOptions& options)
    : host_name_(options.host_name),
      host_jid_(options.host_jid),
      user_name_(options.user_name),
      pin_(options.pin),
      refresh_token_file_path_(options.refresh_token_file_path),
      use_test_environment_(options.use_test_environment) {
  DCHECK(!user_name_.empty());
  DCHECK(!host_name_.empty());
}

ChromotingTestDriverEnvironment::~ChromotingTestDriverEnvironment() = default;

bool ChromotingTestDriverEnvironment::Initialize(
    const std::string& auth_code) {
  if (!access_token_.empty()) {
    return true;
  }

  if (!base::MessageLoopCurrent::Get()) {
    executor_ = std::make_unique<base::SingleThreadTaskExecutor>(
        base::MessagePumpType::IO);
  }

  // If a unit test has set |test_test_token_storage_| then we should use it
  // below.  Note that we do not want to destroy the test object.
  std::unique_ptr<TestTokenStorage> temporary_test_token_storage;
  TestTokenStorage* test_token_storage = test_test_token_storage_;
  if (!test_token_storage) {
    temporary_test_token_storage =
        TestTokenStorage::OnDisk(user_name_, refresh_token_file_path_);
    test_token_storage = temporary_test_token_storage.get();
  }

  // Check to see if we have a refresh token stored for this user.
  refresh_token_ = test_token_storage->FetchRefreshToken();
  if (refresh_token_.empty()) {
    // This isn't necessarily an error as this might be a first run scenario.
    VLOG(2) << "No refresh token stored for " << user_name_;

    if (auth_code.empty()) {
      // No token and no Auth code means no service connectivity, bail!
      LOG(ERROR) << "Cannot retrieve an access token without a stored refresh"
                 << " token on disk or an auth_code passed into the tool";
      return false;
    }
  }

  if (!RetrieveAccessToken(auth_code)) {
    // If we cannot retrieve an access token, then nothing is going to work.
    // Let the caller know that our object is not ready to be used.
    return false;
  }

  return true;
}

void ChromotingTestDriverEnvironment::DisplayHostList() {
  const char kHostAvailabilityFormatString[] = "%-25s%-15s%-35s\n";

  printf(kHostAvailabilityFormatString, "Host Name", "Host Status", "Host JID");
  printf(kHostAvailabilityFormatString, "---------", "-----------", "--------");

  std::string status;
  for (const HostInfo& host_info : host_list_) {
    HostStatus host_status = host_info.status;
    if (host_status == kHostStatusOnline) {
      status = "ONLINE";
    } else if (host_status == kHostStatusOffline) {
      status = "OFFLINE";
    } else {
      status = "UNKNOWN";
    }

    printf(kHostAvailabilityFormatString, host_info.host_name.c_str(),
           status.c_str(), host_info.host_jid.c_str());
  }
}

bool ChromotingTestDriverEnvironment::WaitForHostOnline() {
  if (host_list_.empty()) {
    RetrieveHostList();
  }

  DisplayHostList();

  // Refresh the |host_list_| periodically to check if expected JID is online.
  const base::TimeDelta kTotalTimeInSeconds = base::TimeDelta::FromSeconds(60);
  const base::TimeDelta kSleepTimeInSeconds = base::TimeDelta::FromSeconds(5);
  const int kMaxIterations = kTotalTimeInSeconds / kSleepTimeInSeconds;

  for (int iterations = 0; iterations < kMaxIterations; iterations++) {
    if (!FindHostInHostList()) {
      LOG(WARNING) << "Host '" << host_name_ << "' with JID '" << host_jid_
                   << "' not found in host list.";
      return false;
    }

    if (host_info_.IsReadyForConnection()) {
      if (iterations > 0) {
        VLOG(0) << "Host online after: "
                << iterations * kSleepTimeInSeconds.InSeconds() << " seconds.";
      }
      return true;
    }

    // Wait a while before refreshing host list.
    base::PlatformThread::Sleep(kSleepTimeInSeconds);
    RefreshHostList();
  }

  LOG(ERROR) << "Host '" << host_name_ << "' with JID '" << host_jid_
             << "' still not online after "
             << kMaxIterations * kSleepTimeInSeconds.InSeconds() << " seconds.";
  return false;
}

bool ChromotingTestDriverEnvironment::FindHostInHostList() {
  bool host_found = false;
  for (HostInfo& host_info : host_list_) {
    // The JID is optional so we consider an empty string to be a '*' match.
    bool host_jid_match =
        host_jid_.empty() || (host_jid_ == host_info.host_jid);
    bool host_name_match = host_name_ == host_info.host_name;

    if (host_name_match && host_jid_match) {
      host_info_ = host_info;
      host_found = true;
      break;
    }
  }
  return host_found;
}

void ChromotingTestDriverEnvironment::SetAccessTokenFetcherForTest(
    AccessTokenFetcher* access_token_fetcher) {
  DCHECK(access_token_fetcher);

  test_access_token_fetcher_ = access_token_fetcher;
}

void ChromotingTestDriverEnvironment::SetTestTokenStorageForTest(
    TestTokenStorage* test_token_storage) {
  DCHECK(test_token_storage);

  test_test_token_storage_ = test_token_storage;
}

void ChromotingTestDriverEnvironment::SetHostListFetcherForTest(
    HostListFetcher* host_list_fetcher) {
  DCHECK(host_list_fetcher);

  test_host_list_fetcher_ = host_list_fetcher;
}

void ChromotingTestDriverEnvironment::SetHostNameForTest(
    const std::string& host_name) {
  host_name_ = host_name;
}

void ChromotingTestDriverEnvironment::SetHostJidForTest(
    const std::string& host_jid) {
  host_jid_ = host_jid;
}

void ChromotingTestDriverEnvironment::TearDown() {
  // Letting the MessageLoop tear down during the test destructor results in
  // errors after test completion, when the MessageLoop dtor touches the
  // registered AtExitManager. The AtExitManager is torn down before the test
  // destructor is executed, so we tear down the MessageLoop here, while it is
  // still valid.
  executor_.reset();
}

bool ChromotingTestDriverEnvironment::RetrieveAccessToken(
    const std::string& auth_code) {
  base::RunLoop run_loop;

  access_token_.clear();

  AccessTokenCallback access_token_callback =
      base::BindOnce(&ChromotingTestDriverEnvironment::OnAccessTokenRetrieved,
                     base::Unretained(this), run_loop.QuitClosure());

  // If a unit test has set |test_access_token_fetcher_| then we should use it
  // below.  Note that we do not want to destroy the test object at the end of
  // the function which is why we have the dance below.
  std::unique_ptr<AccessTokenFetcher> temporary_access_token_fetcher;
  AccessTokenFetcher* access_token_fetcher = test_access_token_fetcher_;
  if (!access_token_fetcher) {
    temporary_access_token_fetcher.reset(new AccessTokenFetcher());
    access_token_fetcher = temporary_access_token_fetcher.get();
  }

  if (!auth_code.empty()) {
    // If the user passed in an authcode, then use it to retrieve an
    // updated access/refresh token.
    access_token_fetcher->GetAccessTokenFromAuthCode(
        auth_code, std::move(access_token_callback));
  } else {
    DCHECK(!refresh_token_.empty());

    access_token_fetcher->GetAccessTokenFromRefreshToken(
        refresh_token_, std::move(access_token_callback));
  }

  run_loop.Run();

  // If we were using an auth_code and received a valid refresh token,
  // then we want to store it locally.  If we had an auth code and did not
  // receive a refresh token, then we should let the user know and exit.
  if (!auth_code.empty()) {
    if (!refresh_token_.empty()) {
      // If a unit test has set |test_test_token_storage_| then we should use
      // it below.  Note that we do not want to destroy the test object.
      std::unique_ptr<TestTokenStorage> temporary_test_token_storage;
      TestTokenStorage* test_token_storage = test_test_token_storage_;
      if (!test_token_storage) {
        temporary_test_token_storage =
            TestTokenStorage::OnDisk(user_name_, refresh_token_file_path_);
        test_token_storage = temporary_test_token_storage.get();
      }

      if (!test_token_storage->StoreRefreshToken(refresh_token_)) {
        // If we failed to persist the refresh token, then we should let the
        // user sort out the issue before continuing.
        return false;
      }
    } else {
      LOG(ERROR) << "Failed to use AUTH CODE to retrieve a refresh token.\n"
                 << "Was the one-time use AUTH CODE used more than once?";
      return false;
    }
  }

  if (access_token_.empty()) {
    LOG(ERROR) << "Failed to retrieve access token.";
    return false;
  }

  return true;
}

void ChromotingTestDriverEnvironment::OnAccessTokenRetrieved(
    base::Closure done_closure,
    const std::string& retrieved_access_token,
    const std::string& retrieved_refresh_token) {
  VLOG(1) << "OnAccessTokenRetrieved() Called";
  VLOG(1) << "Access Token: " << retrieved_access_token;

  access_token_ = retrieved_access_token;
  refresh_token_ = retrieved_refresh_token;

  done_closure.Run();
}

bool ChromotingTestDriverEnvironment::RefreshHostList() {
  host_list_.clear();

  return RetrieveHostList();
}

bool ChromotingTestDriverEnvironment::RetrieveHostList() {
  base::RunLoop run_loop;

  // Clear the previous host info.
  host_info_ = HostInfo();

  // If a unit test has set |test_host_list_fetcher_| then we should use it
  // below.  Note that we do not want to destroy the test object at the end of
  // the function which is why we have the dance below.
  std::unique_ptr<HostListFetcher> temporary_host_list_fetcher;
  HostListFetcher* host_list_fetcher = test_host_list_fetcher_;
  if (!host_list_fetcher) {
    temporary_host_list_fetcher.reset(new HostListFetcher());
    host_list_fetcher = temporary_host_list_fetcher.get();
  }

  remoting::test::HostListFetcher::HostlistCallback host_list_callback =
      base::Bind(&ChromotingTestDriverEnvironment::OnHostListRetrieved,
                 base::Unretained(this), run_loop.QuitClosure());

  host_list_fetcher->RetrieveHostlist(
      access_token_,
      use_test_environment_ ? kHostListTestRequestUrl : kHostListProdRequestUrl,
      host_list_callback);

  run_loop.Run();

  if (host_list_.empty()) {
    // Note: Access token may have expired, but it is unlikely.
    LOG(ERROR) << "Retrieved host list is empty.\n"
               << "Does the account have hosts set up?";
    return false;
  }

  return true;
}

void ChromotingTestDriverEnvironment::OnHostListRetrieved(
    base::Closure done_closure,
    const std::vector<HostInfo>& retrieved_host_list) {
  VLOG(1) << "OnHostListRetrieved() Called";

  host_list_ = retrieved_host_list;

  done_closure.Run();
}

}  // namespace test
}  // namespace remoting
