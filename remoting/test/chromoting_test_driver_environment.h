// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_CHROMOTING_TEST_DRIVER_ENVIRONMENT_H_
#define REMOTING_TEST_CHROMOTING_TEST_DRIVER_ENVIRONMENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "remoting/test/host_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class SingleThreadTaskExecutor;
}

namespace remoting {
namespace test {

class AccessTokenFetcher;
class TestTokenStorage;
class HostListFetcher;

// Globally accessible to all test fixtures and cases and has its
// lifetime managed by the GTest framework. It is responsible for managing
// access tokens and retrieving the host list.
class ChromotingTestDriverEnvironment : public testing::Environment {
 public:
  struct EnvironmentOptions {
    EnvironmentOptions();
    ~EnvironmentOptions();

    std::string user_name;
    std::string host_name;
    std::string host_jid;
    std::string pin;
    base::FilePath refresh_token_file_path;
    bool use_test_environment = false;
  };

  explicit ChromotingTestDriverEnvironment(const EnvironmentOptions& options);
  ~ChromotingTestDriverEnvironment() override;

  // Returns false if a valid access token cannot be retrieved.
  bool Initialize(const std::string& auth_code);

  // Clears and then retrieves a new host list.
  bool RefreshHostList();

  // Retrieves connection information for all known hosts and displays
  // their availability to STDOUT.
  void DisplayHostList();

  // Waits for either the host to come online or a maximum timeout. Returns true
  // if host is found online and |host_info_| is valid.
  bool WaitForHostOnline();

  // Used to set fake/mock objects for ChromotingTestDriverEnvironment tests.
  // The caller retains ownership of the supplied objects, and must ensure that
  // they remain valid until the ChromotingTestDriverEnvironment instance has
  // been destroyed.
  void SetAccessTokenFetcherForTest(AccessTokenFetcher* access_token_fetcher);
  void SetTestTokenStorageForTest(TestTokenStorage* test_token_storage);
  void SetHostListFetcherForTest(HostListFetcher* host_list_fetcher);
  void SetHostNameForTest(const std::string& host_name);
  void SetHostJidForTest(const std::string& host_jid);

  // Accessors for fields used by tests.
  const std::string& access_token() const { return access_token_; }
  const std::string& host_name() const { return host_name_; }
  const std::string& pin() const { return pin_; }
  const std::string& user_name() const { return user_name_; }
  bool use_test_environment() const { return use_test_environment_; }
  const std::vector<HostInfo>& host_list() const { return host_list_; }
  const HostInfo& host_info() const { return host_info_; }

 private:
  friend class ChromotingTestDriverEnvironmentTest;

  // testing::Environment interface.
  void TearDown() override;

  // Used to retrieve an access token.  If |auth_code| is empty, then the stored
  // refresh_token will be used instead of |auth_code|.
  // Returns true if a new, valid access token has been retrieved.
  bool RetrieveAccessToken(const std::string& auth_code);

  // Called after the access token fetcher completes.
  // The tokens will be empty on failure.
  void OnAccessTokenRetrieved(base::Closure done_closure,
                              const std::string& retrieved_access_token,
                              const std::string& retrieved_refresh_token);

  // Used to retrieve a host list from the directory service.
  // Returns true if the request was successful and |host_list_| is valid.
  bool RetrieveHostList();

  // Sets |host_info_| if the requested host exists in the host list.
  bool FindHostInHostList();

  // Called after the host info fetcher completes.
  void OnHostListRetrieved(base::Closure done_closure,
                           const std::vector<HostInfo>& retrieved_host_list);

  // Used for authenticating with the directory service.
  std::string access_token_;

  // Used to retrieve an access token.
  std::string refresh_token_;

  // Used to find remote host in host list.
  std::string host_name_;

  // Used to find remote host in host list.
  std::string host_jid_;

  // The test account for a test case.
  std::string user_name_;

  // Used to authenticate a connection with |host_name_|.
  std::string pin_;

  // Path to a JSON file containing refresh tokens.
  base::FilePath refresh_token_file_path_;

  // Indicates whether the test environment APIs should be used.
  const bool use_test_environment_;

  // List of remote hosts for the specified user/test-account.
  std::vector<HostInfo> host_list_;

  // Used to generate connection setup information to connect to |host_name_|.
  HostInfo host_info_;

  // Access token fetcher used by TestDriverEnvironment tests.
  remoting::test::AccessTokenFetcher* test_access_token_fetcher_ = nullptr;

  // TestTokenStorage used by TestDriverEnvironment tests.
  remoting::test::TestTokenStorage* test_test_token_storage_ = nullptr;

  // HostListFetcher used by TestDriverEnvironment tests.
  remoting::test::HostListFetcher* test_host_list_fetcher_ = nullptr;

  // Used for running network request tasks.
  std::unique_ptr<base::SingleThreadTaskExecutor> executor_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingTestDriverEnvironment);
};

// Unfortunately a global var is how the GTEST framework handles sharing data
// between tests and keeping long-lived objects around. Used to share access
// tokens and a host list across tests.
extern ChromotingTestDriverEnvironment* g_chromoting_shared_data;

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_CHROMOTING_TEST_DRIVER_ENVIRONMENT_H_
