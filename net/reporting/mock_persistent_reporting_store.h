// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_MOCK_PERSISTENT_REPORTING_STORE_H_
#define NET_REPORTING_MOCK_PERSISTENT_REPORTING_STORE_H_

#include <vector>

#include "base/functional/callback.h"
#include "net/base/network_anonymization_key.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_endpoint.h"
#include "url/origin.h"

namespace net {

// A ReportingCache::PersistentReportingStore implementation that stashes the
// received commands in order in a vector, to be checked by tests. Simulates
// loading pre-existing stored endpoints and endpoint groups, which can be
// provided using SetPrestoredClients().
//
// TODO(sburnett): Replace this with a fake store to reduce awkwardness.
class MockPersistentReportingStore
    : public ReportingCache::PersistentReportingStore {
 public:
  struct Command {
    enum class Type {
      LOAD_REPORTING_CLIENTS,
      ADD_REPORTING_ENDPOINT,
      ADD_REPORTING_ENDPOINT_GROUP,
      UPDATE_REPORTING_ENDPOINT_GROUP_ACCESS_TIME,
      UPDATE_REPORTING_ENDPOINT_DETAILS,
      UPDATE_REPORTING_ENDPOINT_GROUP_DETAILS,
      DELETE_REPORTING_ENDPOINT,
      DELETE_REPORTING_ENDPOINT_GROUP,
      FLUSH
    };

    // Constructor for LOAD_REPORTING_CLIENTS commands.
    Command(Type type, ReportingClientsLoadedCallback loaded_callback);
    // Constructors for endpoint commands. |type| must be one of
    // ADD_REPORTING_ENDPOINT, UPDATE_REPORTING_ENDPOINT_DETAILS, or
    // DELETE_REPORTING_ENDPOINT
    Command(Type type, const ReportingEndpoint& endpoint);
    Command(Type type,
            const ReportingEndpointGroupKey& group_key,
            const GURL& endpoint_url);
    // Constructors for endpoint group commands. |type| must be one of
    // ADD_REPORTING_ENDPOINT_GROUP,
    // UPDATE_REPORTING_ENDPOINT_GROUP_ACCESS_TIME,
    // UPDATE_REPORTING_ENDPOINT_GROUP_DETAILS, or
    // DELETE_REPORTING_ENDPOINT_GROUP
    Command(Type type, const CachedReportingEndpointGroup& group);
    Command(Type type, const ReportingEndpointGroupKey& group_key);
    // |type| must be LOAD_REPORTING_CLIENTS or FLUSH.
    explicit Command(Type type);

    Command(const Command& other);
    Command(Command&& other);

    ~Command();

    // Type of command.
    Type type;

    // Identifies the group to which the command pertains. (Applies to endpoint
    // and endpoint group commands.)
    ReportingEndpointGroupKey group_key = ReportingEndpointGroupKey();

    // Identifies the endpoint to which the command pertains. (Applies to
    // endpoint commands only.)
    GURL url;

    // The supplied callback to be run when loading is complete. (Only applies
    // for load commands.)
    ReportingClientsLoadedCallback loaded_callback;
  };

  using CommandList = std::vector<Command>;

  MockPersistentReportingStore();

  MockPersistentReportingStore(const MockPersistentReportingStore&) = delete;
  MockPersistentReportingStore& operator=(const MockPersistentReportingStore&) =
      delete;

  ~MockPersistentReportingStore() override;

  // PersistentReportingStore implementation:
  void LoadReportingClients(
      ReportingClientsLoadedCallback loaded_callback) override;
  void AddReportingEndpoint(const ReportingEndpoint& endpoint) override;
  void AddReportingEndpointGroup(
      const CachedReportingEndpointGroup& group) override;
  void UpdateReportingEndpointGroupAccessTime(
      const CachedReportingEndpointGroup& group) override;
  void UpdateReportingEndpointDetails(
      const ReportingEndpoint& endpoint) override;
  void UpdateReportingEndpointGroupDetails(
      const CachedReportingEndpointGroup& group) override;
  void DeleteReportingEndpoint(const ReportingEndpoint& endpoint) override;
  void DeleteReportingEndpointGroup(
      const CachedReportingEndpointGroup& group) override;
  void Flush() override;

  // Simulates pre-existing clients that were stored previously. Should only be
  // called once, at the beginning of the test before any other method calls.
  void SetPrestoredClients(std::vector<ReportingEndpoint> endpoints,
                           std::vector<CachedReportingEndpointGroup> groups);

  // Simulate finishing loading clients by executing the loaded_callback of the
  // first LOAD_REPORTING_CLIENTS command (which should also be the only
  // LOAD_REPORTING_CLIENTS command). If |load_success| is false, the vectors of
  // endpoints and groups passed to the callback will be empty. If
  // |load_success| is true, the prestored clients will be passed to the
  // callback.
  void FinishLoading(bool load_success);

  // Verify that |command_list_| matches |expected_commands|.
  // TODO(sburnett): Replace this with a set of gmock matchers.
  bool VerifyCommands(const CommandList& expected_commands) const;

  // Count the number of commands with type |t|.
  int CountCommands(Command::Type t);

  void ClearCommands();

  CommandList GetAllCommands() const;

  // Gets the number of stored endpoints/groups, simulating the actual number
  // that would be written on disk if this were a real store.
  int StoredEndpointsCount() const { return endpoint_count_; }
  int StoredEndpointGroupsCount() const { return endpoint_group_count_; }

 private:
  // List of commands that we have received so far.
  CommandList command_list_;

  // Simulated pre-existing stored clients.
  std::vector<ReportingEndpoint> prestored_endpoints_;
  std::vector<CachedReportingEndpointGroup> prestored_endpoint_groups_;

  // Set when LoadReportingClients() is called.
  bool load_started_ = false;

  // Simulates the total number of endpoints/groups that would be stored in the
  // store. Updated when pre-stored policies are added, and when Flush() is
  // called.
  int endpoint_count_ = 0;
  int endpoint_group_count_ = 0;

  // Simulates the delta to be added to to the counts the next time Flush() is
  // called. Reset to 0 when Flush() is called.
  int queued_endpoint_count_delta_ = 0;
  int queued_endpoint_group_count_delta_ = 0;
};

bool operator==(const MockPersistentReportingStore::Command& lhs,
                const MockPersistentReportingStore::Command& rhs);
bool operator!=(const MockPersistentReportingStore::Command& lhs,
                const MockPersistentReportingStore::Command& rhs);
std::ostream& operator<<(std::ostream& out,
                         const MockPersistentReportingStore::Command& cmd);

}  // namespace net

#endif  // NET_REPORTING_MOCK_PERSISTENT_REPORTING_STORE_H_
