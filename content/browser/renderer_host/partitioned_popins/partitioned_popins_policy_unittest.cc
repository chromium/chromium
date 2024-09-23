// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/partitioned_popins/partitioned_popins_policy.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

// Test the parsing of partitioned-popin policies.
TEST(PartitionedPopinsPolicy, Parse) {
  struct {
    std::string description;
    std::string untrusted_input;
    bool wildcard;
    std::vector<url::Origin> origins;
  } test_cases[] = {
      {
          "insecure origin",
          "partitioned=(\"http://demo.example/\")",
          false,
          {},
      },
      {
          "malformed policy",
          "something;222",
          false,
          {},
      },
      {
          "empty policy",
          "partitioned=()",
          false,
          {},
      },
      {
          "empty string",
          "",
          false,
          {},
      },
      {
          "one origin",
          "partitioned=(\"https://demo.example/\")",
          false,
          {
              url::Origin::Create(GURL("https://demo.example/")),
          },
      },
      {
          "multiple origins",
          "partitioned=(\"https://demo.example/\" \"https://test.example2/\")",
          false,
          {
              url::Origin::Create(GURL("https://demo.example/")),
              url::Origin::Create(GURL("https://test.example2/")),
          },
      },
      {
          "other policies ignored",
          "partitioned=*,unpartitioned=()",
          true,
          {},
      },
      {
          "origin path ignored",
          "partitioned=(\"https://demo.example/path/\")",
          false,
          {
              url::Origin::Create(GURL("https://demo.example/")),
          },
      },
      {
          "unquoted origin",
          "partitioned=(https://demo.example/)",
          false,
          {},
      },
      {
          "wildcard origin policy",
          "partitioned=(* \"https://demo.example/\")",
          true,
          {},
      },
      {
          "wildcard policy",
          "partitioned=*",
          true,
          {},
      },
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    PartitionedPopinsPolicy policy(test_case.untrusted_input);
    EXPECT_EQ(test_case.wildcard, policy.wildcard);
    EXPECT_EQ(test_case.origins, policy.origins);
  }
}

}  // namespace content
