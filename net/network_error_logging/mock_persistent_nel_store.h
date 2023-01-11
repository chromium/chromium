// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NETWORK_ERROR_LOGGING_MOCK_PERSISTENT_NEL_STORE_H_
#define NET_NETWORK_ERROR_LOGGING_MOCK_PERSISTENT_NEL_STORE_H_

#include <vector>

#include "base/functional/callback.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "url/origin.h"

namespace net {

// A NetworkErrorLoggingService::PersistentNelStore implementation that stashes
// the received commands in order in a vector, to be checked by tests.
// Simulates loading pre-existing stored policies, which can be provided
// using SetLoadExpectation().
class MockPersistentNelStore
    : public NetworkErrorLoggingService::PersistentNelStore {
 public:
  // Represents a command that has been passed to the MockPersistentNelStore.
  struct Command {
    enum class Type {
      LOAD_NEL_POLICIES,
      ADD_NEL_POLICY,
      UPDATE_NEL_POLICY,
      DELETE_NEL_POLICY,
      FLUSH
    };

    // Constructor for LOAD_NEL_POLICIES commands.
    Command(Type type, NelPoliciesLoadedCallback loaded_callback);
    // Constructor for ADD_NEL_POLICY, UPDATE_NEL_POLICY, and DELETE_NEL_POLICY
    // commands.
    Command(Type type, const NetworkErrorLoggingService::NelPolicy& policy);
    // Constructor for FLUSH commands.
    explicit Command(Type type);

    Command(const Command& other);
    Command(Command&& other);

    ~Command();

    // Type of command.
    Type type;

    // The key of the policy that the command pertains to. (Only applies for
    // add, update, and delete)
    NetworkErrorLoggingService::NelPolicyKey key;

    // The supplied callback to be run when loading is complete. (Only applies
    // for load commands).
    NelPoliciesLoadedCallback loaded_callback;
  };

  using CommandList = std::vector<Command>;

  MockPersistentNelStore();

  MockPersistentNelStore(const MockPersistentNelStore&) = delete;
  MockPersistentNelStore& operator=(const MockPersistentNelStore&) = delete;

  ~MockPersistentNelStore() override;

  // PersistentNelStore implementation:
  void LoadNelPolicies(NelPoliciesLoadedCallback loaded_callback) override;
  void AddNelPolicy(
      const NetworkErrorLoggingService::NelPolicy& policy) override;
  void UpdateNelPolicyAccessTime(
      const NetworkErrorLoggingService::NelPolicy& policy) override;
  void DeleteNelPolicy(
      const NetworkErrorLoggingService::NelPolicy& policy) override;
  void Flush() override;

  // Simulates pre-existing policies that were stored previously. Should only be
  // called once, at the beginning of the test before any other method calls.
  void SetPrestoredPolicies(
      std::vector<NetworkErrorLoggingService::NelPolicy> policies);

  // Simulate finishing loading policies by executing the loaded_callback of the
  // first LOAD_NEL_POLICIES command (which should also be the only
  // LOAD_NEL_POLICIES command). If |load_success| is false, the vector of
  // policies passed to the callback will be empty.  If |load_success| is true,
  // the vector of policies passed to the callback will be
  // |prestored_policies_|.
  void FinishLoading(bool load_success);

  // Verify that |command_list_| matches |expected_commands|.
  bool VerifyCommands(const CommandList& expected_commands) const;

  CommandList GetAllCommands() const;

  // Returns the total number of policies that would be stored in the store, if
  // this were a real store.
  int StoredPoliciesCount() const { return policy_count_; }

 private:
  // List of commands that we have received so far.
  CommandList command_list_;

  // Simulated pre-existing stored policies.
  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies_;

  // Set when LoadNelPolicies() is called.
  bool load_started_ = false;

  // Simulates the total number of policies that would be stored in the store.
  // Updated when pre-stored policies are added, and when Flush() is called.
  int policy_count_ = 0;

  // Simulates the delta to be added to |policy_count_| the next time Flush() is
  // called. Reset to 0 when Flush() is called.
  int queued_policy_count_delta_ = 0;
};

bool operator==(const MockPersistentNelStore::Command& lhs,
                const MockPersistentNelStore::Command& rhs);
bool operator!=(const MockPersistentNelStore::Command& lhs,
                const MockPersistentNelStore::Command& rhs);

}  // namespace net

#endif  // NET_NETWORK_ERROR_LOGGING_MOCK_PERSISTENT_NEL_STORE_H_
