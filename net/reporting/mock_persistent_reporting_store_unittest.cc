// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/mock_persistent_reporting_store.h"

#include <vector>

#include "base/location.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "net/reporting/reporting_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {

using CommandType = MockPersistentReportingStore::Command::Type;

const url::Origin kOrigin = url::Origin::Create(GURL("https://example.test/"));
const char kGroupName[] = "groupname";
const GURL kUrl = GURL("https://endpoint.test/reports");
const ReportingEndpoint kEndpoint(kOrigin, kGroupName, {kUrl});
const CachedReportingEndpointGroup kGroup(kOrigin,
                                          kGroupName,
                                          OriginSubdomains::DEFAULT,
                                          base::Time::Now() +
                                              base::TimeDelta::FromDays(1),
                                          base::Time::Now());

void RunClosureOnClientsLoaded(
    base::OnceClosure closure,
    std::vector<ReportingEndpoint>* endpoints_out,
    std::vector<CachedReportingEndpointGroup>* groups_out,
    std::vector<ReportingEndpoint> loaded_endpoints,
    std::vector<CachedReportingEndpointGroup> loaded_groups) {
  std::move(closure).Run();
  loaded_endpoints.swap(*endpoints_out);
  loaded_groups.swap(*groups_out);
}

// Makes a ReportingClientsLoadedCallback that will fail if it's never run
// before destruction.
MockPersistentReportingStore::ReportingClientsLoadedCallback
MakeExpectedRunReportingClientsLoadedCallback(
    std::vector<ReportingEndpoint>* endpoints_out,
    std::vector<CachedReportingEndpointGroup>* groups_out) {
  base::OnceClosure closure = base::MakeExpectedRunClosure(FROM_HERE);
  return base::BindOnce(&RunClosureOnClientsLoaded, std::move(closure),
                        endpoints_out, groups_out);
}

// Test that FinishLoading() runs the callback.
TEST(MockPersistentReportingStoreTest, FinishLoading) {
  MockPersistentReportingStore store;
  MockPersistentReportingStore::CommandList expected_commands;
  std::vector<ReportingEndpoint> loaded_endpoints;
  std::vector<CachedReportingEndpointGroup> loaded_groups;

  store.LoadReportingClients(MakeExpectedRunReportingClientsLoadedCallback(
      &loaded_endpoints, &loaded_groups));
  expected_commands.emplace_back(CommandType::LOAD_REPORTING_CLIENTS);

  store.FinishLoading(true /* load_success */);
  EXPECT_EQ(0u, loaded_endpoints.size());
  EXPECT_EQ(0u, loaded_groups.size());

  EXPECT_TRUE(store.VerifyCommands(expected_commands));
  // Test should not crash because the callback has been run.
}

TEST(MockPersistentReportingStoreTest, PreStoredClients) {
  MockPersistentReportingStore store;
  MockPersistentReportingStore::CommandList expected_commands;
  std::vector<ReportingEndpoint> loaded_endpoints;
  std::vector<CachedReportingEndpointGroup> loaded_groups;

  store.SetPrestoredClients({kEndpoint}, {kGroup});
  EXPECT_EQ(1, store.StoredEndpointsCount());
  EXPECT_EQ(1, store.StoredEndpointGroupsCount());

  store.LoadReportingClients(MakeExpectedRunReportingClientsLoadedCallback(
      &loaded_endpoints, &loaded_groups));
  expected_commands.emplace_back(CommandType::LOAD_REPORTING_CLIENTS);

  store.FinishLoading(true /* load_success */);
  EXPECT_EQ(1u, loaded_endpoints.size());
  EXPECT_EQ(1u, loaded_groups.size());

  EXPECT_TRUE(store.VerifyCommands(expected_commands));
}

// Failed load should yield empty vectors of endpoints and endpoint groups.
TEST(MockPersistentReportingStoreTest, FailedLoad) {
  MockPersistentReportingStore store;
  MockPersistentReportingStore::CommandList expected_commands;
  std::vector<ReportingEndpoint> loaded_endpoints;
  std::vector<CachedReportingEndpointGroup> loaded_groups;

  store.SetPrestoredClients({kEndpoint}, {kGroup});
  EXPECT_EQ(1, store.StoredEndpointsCount());
  EXPECT_EQ(1, store.StoredEndpointGroupsCount());

  store.LoadReportingClients(MakeExpectedRunReportingClientsLoadedCallback(
      &loaded_endpoints, &loaded_groups));
  expected_commands.emplace_back(CommandType::LOAD_REPORTING_CLIENTS);

  store.FinishLoading(false /* load_success */);
  EXPECT_EQ(0u, loaded_endpoints.size());
  EXPECT_EQ(0u, loaded_groups.size());

  EXPECT_TRUE(store.VerifyCommands(expected_commands));
}

TEST(MockPersistentReportingStoreTest, AddFlushDeleteFlush) {
  MockPersistentReportingStore store;
  MockPersistentReportingStore::CommandList expected_commands;
  std::vector<ReportingEndpoint> loaded_endpoints;
  std::vector<CachedReportingEndpointGroup> loaded_groups;

  store.LoadReportingClients(MakeExpectedRunReportingClientsLoadedCallback(
      &loaded_endpoints, &loaded_groups));
  expected_commands.emplace_back(CommandType::LOAD_REPORTING_CLIENTS);
  EXPECT_EQ(1u, store.GetAllCommands().size());

  store.FinishLoading(true /* load_success */);
  EXPECT_EQ(0u, loaded_endpoints.size());
  EXPECT_EQ(0u, loaded_groups.size());
  EXPECT_EQ(0, store.StoredEndpointsCount());
  EXPECT_EQ(0, store.StoredEndpointGroupsCount());

  store.AddReportingEndpoint(kEndpoint);
  expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                 kEndpoint);
  EXPECT_EQ(2u, store.GetAllCommands().size());

  store.AddReportingEndpointGroup(kGroup);
  expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                 kGroup);
  EXPECT_EQ(3u, store.GetAllCommands().size());

  store.Flush();
  expected_commands.emplace_back(CommandType::FLUSH);
  EXPECT_EQ(4u, store.GetAllCommands().size());
  EXPECT_EQ(1, store.StoredEndpointsCount());
  EXPECT_EQ(1, store.StoredEndpointGroupsCount());

  store.DeleteReportingEndpoint(kEndpoint);
  expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                 kEndpoint);
  EXPECT_EQ(5u, store.GetAllCommands().size());

  store.DeleteReportingEndpointGroup(kGroup);
  expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                 kGroup);
  EXPECT_EQ(6u, store.GetAllCommands().size());

  store.Flush();
  expected_commands.emplace_back(CommandType::FLUSH);
  EXPECT_EQ(7u, store.GetAllCommands().size());
  EXPECT_EQ(0, store.StoredEndpointsCount());
  EXPECT_EQ(0, store.StoredEndpointGroupsCount());

  EXPECT_TRUE(store.VerifyCommands(expected_commands));

  EXPECT_EQ(1, store.CountCommands(CommandType::LOAD_REPORTING_CLIENTS));
  EXPECT_EQ(
      0, store.CountCommands(CommandType::UPDATE_REPORTING_ENDPOINT_DETAILS));
}

TEST(MockPersistentReportingStoreTest, CountCommands) {
  MockPersistentReportingStore store;

  std::vector<ReportingEndpoint> loaded_endpoints;
  std::vector<CachedReportingEndpointGroup> loaded_groups;
  store.LoadReportingClients(MakeExpectedRunReportingClientsLoadedCallback(
      &loaded_endpoints, &loaded_groups));
  store.FinishLoading(true /* load_success */);

  store.AddReportingEndpoint(kEndpoint);
  store.AddReportingEndpointGroup(kGroup);
  store.Flush();

  store.DeleteReportingEndpoint(kEndpoint);
  store.DeleteReportingEndpointGroup(kGroup);
  store.Flush();

  EXPECT_EQ(1, store.CountCommands(CommandType::LOAD_REPORTING_CLIENTS));
  EXPECT_EQ(1, store.CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
  EXPECT_EQ(1, store.CountCommands(CommandType::ADD_REPORTING_ENDPOINT_GROUP));
  EXPECT_EQ(0, store.CountCommands(
                   CommandType::UPDATE_REPORTING_ENDPOINT_GROUP_ACCESS_TIME));
  EXPECT_EQ(
      0, store.CountCommands(CommandType::UPDATE_REPORTING_ENDPOINT_DETAILS));
  EXPECT_EQ(0, store.CountCommands(
                   CommandType::UPDATE_REPORTING_ENDPOINT_GROUP_DETAILS));
  EXPECT_EQ(1, store.CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
  EXPECT_EQ(1,
            store.CountCommands(CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
  EXPECT_EQ(2, store.CountCommands(CommandType::FLUSH));
}

}  // namespace

}  // namespace net
