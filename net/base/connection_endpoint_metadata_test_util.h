// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CONNECTION_ENDPOINT_METADATA_TEST_UTIL_H_
#define NET_BASE_CONNECTION_ENDPOINT_METADATA_TEST_UTIL_H_

#include <ostream>
#include <string>
#include <vector>

#include "net/base/connection_endpoint_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

testing::Matcher<const ConnectionEndpointMetadata&>
ExpectConnectionEndpointMetadata(
    testing::Matcher<std::vector<std::string>>
        supported_protocol_alpns_matcher = testing::IsEmpty(),
    testing::Matcher<ConnectionEndpointMetadata::EchConfigList>
        ech_config_list_matcher = testing::IsEmpty(),
    testing::Matcher<std::string> target_name_matcher = testing::IsEmpty());

std::ostream& operator<<(
    std::ostream& os,
    const ConnectionEndpointMetadata& connection_endpoint_metadata);

}  // namespace net

#endif  // NET_BASE_CONNECTION_ENDPOINT_METADATA_TEST_UTIL_H_
