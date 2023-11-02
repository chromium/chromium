// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/client_service_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

using MapDataType = size_t;
using ClientServiceMapType = ClientServiceMap<MapDataType, MapDataType>;

TEST(ClientServiceMap, BasicMapping) {
  static constexpr MapDataType kClientId = 1;
  static constexpr MapDataType kServiceId = 2;
  ClientServiceMapType map;

  // Add just one mapping
  map.SetIDMapping(kClientId, kServiceId);

  // Check client -> service ID lookup
  MapDataType service_id = 0;
  EXPECT_TRUE(map.GetServiceID(kClientId, &service_id));
  EXPECT_EQ(kServiceId, service_id);

  // Check null is handled for GetServiceID
  EXPECT_TRUE(map.HasClientID(kClientId));

  // Check service -> client ID lookup
  MapDataType client_id = 0;
  EXPECT_TRUE(map.GetClientID(kServiceId, &client_id));
  EXPECT_EQ(kClientId, client_id);

  // Check null is handled for GetClientID
  EXPECT_TRUE(map.GetClientID(kServiceId, nullptr));
}

TEST(ClientServiceMap, ZeroID) {
  static constexpr MapDataType kServiceId = 2;
  ClientServiceMapType map;

  // Check that the zero client ID always maps to the zero service ID
  MapDataType service_id = 0;
  EXPECT_TRUE(map.GetServiceID(0, &service_id));
  EXPECT_EQ(0u, service_id);

  // Check that it's possible to update the zero client ID
  map.SetIDMapping(0, kServiceId);
  EXPECT_TRUE(map.GetServiceID(0, &service_id));
  EXPECT_EQ(kServiceId, service_id);
}

TEST(ClientServiceMap, InvalidIDs) {
  static constexpr MapDataType kClientId = 1;
  static constexpr MapDataType kServiceId = 2;
  ClientServiceMapType map;

  // Add just one mapping
  map.SetIDMapping(kClientId, kServiceId);

  // Check that GetServiceIDOrInvalid returns the correct ID or the invalid ID
  EXPECT_EQ(kServiceId, map.GetServiceIDOrInvalid(kClientId));
  EXPECT_EQ(map.invalid_service_id(), map.GetServiceIDOrInvalid(kClientId + 1));

  // Check that GetClientID returns false if the ID does not exist
  EXPECT_FALSE(map.GetClientID(kServiceId + 1, nullptr));
}

TEST(ClientServiceMap, UpdateMapping) {
  static constexpr MapDataType kClientId = 1;
  static constexpr MapDataType kServiceId = 2;
  static constexpr MapDataType kNewServiceId =
      ClientServiceMapType::kMaxFlatArraySize + 10;
  ClientServiceMapType map;

  map.SetIDMapping(kClientId, kServiceId);
  EXPECT_EQ(kServiceId, map.GetServiceIDOrInvalid(kClientId));

  map.RemoveClientID(kClientId);
  map.SetIDMapping(kClientId, kNewServiceId);
  EXPECT_EQ(kNewServiceId, map.GetServiceIDOrInvalid(kClientId));
}

TEST(ClientServiceMap, RemoveMapping) {
  static constexpr MapDataType kClientId = 1;
  static constexpr MapDataType kServiceId = 2;
  static constexpr MapDataType kLargeServiceId =
      ClientServiceMapType::kMaxFlatArraySize + 10;
  ClientServiceMapType map;

  map.SetIDMapping(kClientId, kServiceId);
  EXPECT_TRUE(map.HasClientID(kClientId));
  map.RemoveClientID(kClientId);
  EXPECT_FALSE(map.HasClientID(kClientId));

  map.SetIDMapping(kClientId, kServiceId);
  EXPECT_TRUE(map.HasClientID(kClientId));
  map.Clear();
  EXPECT_FALSE(map.HasClientID(kClientId));

  map.SetIDMapping(kClientId, kLargeServiceId);
  EXPECT_TRUE(map.HasClientID(kClientId));
  map.RemoveClientID(kClientId);
  EXPECT_FALSE(map.HasClientID(kClientId));
}

TEST(ClientServiceMap, ManyIDs) {
  static constexpr MapDataType kFirstClientId = 1;
  static constexpr MapDataType kFirstServiceId = 2;

  // Make sure to cover the transition from a flat array to unordered map
  static constexpr size_t kIdStep = 100;
  static constexpr size_t kIdCount =
      (ClientServiceMapType::kMaxFlatArraySize + 10) / kIdStep;

  // Insert kIdCount mappings
  ClientServiceMapType map;
  for (size_t ii = 0; ii < kIdCount; ii++) {
    map.SetIDMapping(kFirstClientId + (ii * kIdStep),
                     kFirstServiceId + (ii * kIdStep));
  }

  // Check each mapping
  for (size_t ii = 0; ii < kIdCount; ii++) {
    // Check client -> service ID lookup
    EXPECT_EQ(kFirstServiceId + (ii * kIdStep),
              map.GetServiceIDOrInvalid(kFirstClientId + (ii * kIdStep)));

    // Check service -> client ID lookup
    MapDataType client_id = 0;
    EXPECT_TRUE(map.GetClientID(kFirstServiceId + (ii * kIdStep), &client_id));
    EXPECT_EQ(kFirstClientId + (ii * kIdStep), client_id);
  }
}

TEST(ClientServiceMap, DuplicateServiceIDs) {
  static constexpr MapDataType kFirstClientId = 1;
  static constexpr MapDataType kSecondClientId =
      ClientServiceMapType::kMaxFlatArraySize + 1000;
  static constexpr MapDataType kServiceId = 2;

  ClientServiceMapType map;

  // Two client IDs point to the same service ID.
  map.SetIDMapping(kFirstClientId, kServiceId);
  map.SetIDMapping(kSecondClientId, kServiceId);

  // Confirm that GetServiceID works in this case.
  EXPECT_EQ(kServiceId, map.GetServiceIDOrInvalid(kFirstClientId));
  EXPECT_EQ(kServiceId, map.GetServiceIDOrInvalid(kSecondClientId));

  // GetClientID should return the first client ID.
  MapDataType client_id = 0;
  EXPECT_TRUE(map.GetClientID(kServiceId, &client_id));
  EXPECT_EQ(kFirstClientId, client_id);
}

TEST(ClientServiceMap, ForEach) {
  using MappingSet = std::set<std::pair<MapDataType, MapDataType>>;
  const MappingSet kMappings = {
      {0, 1},
      {10, 10},
      {100, 100},
      {ClientServiceMapType::kMaxFlatArraySize - 1, 15},
      {ClientServiceMapType::kMaxFlatArraySize, 5},
      {ClientServiceMapType::kMaxFlatArraySize + 1000, 50},
  };

  ClientServiceMapType map;
  for (const auto& mapping : kMappings) {
    map.SetIDMapping(mapping.first, mapping.second);
  }

  MappingSet seen_mappings;
  map.ForEach([&seen_mappings](MapDataType client_id, MapDataType service_id) {
    seen_mappings.insert(std::make_pair(client_id, service_id));
  });
  EXPECT_EQ(kMappings, seen_mappings);
}

}  // namespace gles2
}  // namespace gpu
