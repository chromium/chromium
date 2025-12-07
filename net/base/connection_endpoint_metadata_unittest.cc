// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/connection_endpoint_metadata.h"

#include <set>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(ConnectionEndpointMetadataTest, Set) {
  auto metadatas = std::to_array<ConnectionEndpointMetadata>({
      {/*supported_protocol_alpns=*/{{"h2"}},
       /*ech_config_list=*/{},
       /*target_name=*/"a.test",
       /*trust_anchor_ids=*/{}},
      {/*supported_protocol_alpns=*/{{"h3"}},
       /*ech_config_list=*/{},
       /*target_name=*/"a.test",
       /*trust_anchor_ids=*/{}},
      {/*supported_protocol_alpns=*/{{"h2"}},
       /*ech_config_list=*/{0x01},
       /*target_name=*/"a.test",
       /*trust_anchor_ids=*/{}},
      {/*supported_protocol_alpns=*/{{"h2"}},
       /*ech_config_list=*/{0x02},
       /*target_name=*/"a.test",
       /*trust_anchor_ids=*/{}},
      {/*supported_protocol_alpns=*/{{"h2"}},
       /*ech_config_list=*/{0x01},
       /*target_name=*/"a.test",
       /*trust_anchor_ids=*/{{0x01}}},
      {/*supported_protocol_alpns=*/{{"h2"}},
       /*ech_config_list=*/{0x01},
       /*target_name=*/"a.test",
       /*trust_anchor_ids=*/{{0x02}}},
      {/*supported_protocol_alpns=*/{{"h2"}},
       /*ech_config_list=*/{},
       /*target_name=*/"b.test",
       /*trust_anchor_ids=*/{}},
  });

  std::set<ConnectionEndpointMetadata> metadata_set(metadatas.begin(),
                                                    metadatas.end());
  ASSERT_EQ(metadatas.size(), metadata_set.size());
}

}  // namespace net
