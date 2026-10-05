// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/http_request_headers_update_params.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/headers_matcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;

TEST(HttpRequestHeadersUpdateParamsTest, Clear) {
  HttpRequestHeadersUpdateParams params;
  params.removed_headers = {"Header-A"};
  params.modified_headers.SetHeader("Header-B", "value-b");
  params.modified_cors_exempt_headers.SetHeader("Header-C", "value-c");

  params.Clear();

  EXPECT_TRUE(params.removed_headers.empty());
  EXPECT_TRUE(params.modified_headers.IsEmpty());
  EXPECT_TRUE(params.modified_cors_exempt_headers.IsEmpty());
}

TEST(HttpRequestHeadersUpdateParamsTest, ApplyEmptyParams) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Header-A", "value-a");
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader("Header-B", "value-b");
  const std::string original_headers_str = headers.ToString();
  const std::string original_cors_exempt_str = cors_exempt_headers.ToString();

  HttpRequestHeadersUpdateParams params;
  params.Apply(headers, cors_exempt_headers);

  EXPECT_EQ(headers.ToString(), original_headers_str);
  EXPECT_EQ(cors_exempt_headers.ToString(), original_cors_exempt_str);
}

TEST(HttpRequestHeadersUpdateParamsTest, ApplyRemovedHeaders) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Header-A", "value-a");
  headers.SetHeader("Header-B", "value-b");
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader("Header-B", "cors-value-b");
  cors_exempt_headers.SetHeader("Header-C", "value-c");

  HttpRequestHeadersUpdateParams params;
  params.removed_headers = {"Header-B", "Header-Non-Existent"};
  params.Apply(headers, cors_exempt_headers);

  EXPECT_TRUE(headers.HasHeader("Header-A"));
  EXPECT_FALSE(headers.HasHeader("Header-B"));
  EXPECT_FALSE(cors_exempt_headers.HasHeader("Header-B"));
  EXPECT_TRUE(cors_exempt_headers.HasHeader("Header-C"));
}

TEST(HttpRequestHeadersUpdateParamsTest, ApplyModifiedHeaders) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Header-A", "old-a");
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader("Header-B", "old-b");

  HttpRequestHeadersUpdateParams params;
  params.modified_headers.SetHeader("Header-A", "new-a");
  params.modified_headers.SetHeader("Header-C", "new-c");
  params.modified_cors_exempt_headers.SetHeader("Header-B", "new-b");
  params.modified_cors_exempt_headers.SetHeader("Header-D", "new-d");
  params.Apply(headers, cors_exempt_headers);

  EXPECT_THAT(headers.GetHeader("Header-A"), Optional(std::string("new-a")));
  EXPECT_THAT(headers.GetHeader("Header-C"), Optional(std::string("new-c")));
  EXPECT_FALSE(headers.HasHeader("Header-B"));
  EXPECT_FALSE(headers.HasHeader("Header-D"));
  EXPECT_THAT(cors_exempt_headers.GetHeader("Header-B"),
              Optional(std::string("new-b")));
  EXPECT_THAT(cors_exempt_headers.GetHeader("Header-D"),
              Optional(std::string("new-d")));
  EXPECT_FALSE(cors_exempt_headers.HasHeader("Header-A"));
  EXPECT_FALSE(cors_exempt_headers.HasHeader("Header-C"));
}

TEST(HttpRequestHeadersUpdateParamsTest, IsCaseInsensitive) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Header-A", "value-a");
  headers.SetHeader("Header-B", "value-b");
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader("Header-B", "cors-value-b");
  cors_exempt_headers.SetHeader("Header-C", "value-c");

  HttpRequestHeadersUpdateParams params;
  params.removed_headers = {"header-b"};
  params.modified_headers.SetHeader("header-a", "new-a");
  params.modified_cors_exempt_headers.SetHeader("header-c", "new-c");
  params.Apply(headers, cors_exempt_headers);

  EXPECT_THAT(headers.GetHeader("Header-A"), Optional(std::string("new-a")));
  EXPECT_FALSE(headers.HasHeader("Header-B"));
  EXPECT_FALSE(cors_exempt_headers.HasHeader("Header-B"));
  EXPECT_THAT(cors_exempt_headers.GetHeader("Header-C"),
              Optional(std::string("new-c")));
}

TEST(HttpRequestHeadersUpdateParamsTest, ApplyRemovalBeforeModification) {
  // If the same header is specified in removed_headers and modified_headers,
  // removal happens first, so the modified value should persist.
  net::HttpRequestHeaders headers;
  headers.SetHeader("Header-A", "old-a");
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader("Header-B", "old-b");

  HttpRequestHeadersUpdateParams params;
  params.removed_headers = {"Header-A", "Header-B"};
  params.modified_headers.SetHeader("Header-A", "new-a");
  params.modified_cors_exempt_headers.SetHeader("Header-B", "new-b");
  params.Apply(headers, cors_exempt_headers);

  EXPECT_THAT(headers.GetHeader("Header-A"), Optional(std::string("new-a")));
  EXPECT_FALSE(headers.HasHeader("Header-B"));
  EXPECT_THAT(cors_exempt_headers.GetHeader("Header-B"),
              Optional(std::string("new-b")));
  EXPECT_FALSE(cors_exempt_headers.HasHeader("Header-A"));
}

TEST(HttpRequestHeadersUpdateParamsTest, MergeFrom) {
  HttpRequestHeadersUpdateParams a;
  a.removed_headers = {"Removed-A", "Removed-Shared"};
  a.modified_headers.SetHeader("Set-A", "val-a");
  a.modified_headers.SetHeader("Set-Shared", "val-shared-a");
  a.modified_cors_exempt_headers.SetHeader("Set-Cors-A", "val-cors-a");
  a.modified_cors_exempt_headers.SetHeader("Set-Cors-Shared",
                                           "val-cors-shared-a");
  HttpRequestHeadersUpdateParams b;
  b.removed_headers = {"Removed-Shared", "Removed-B"};
  b.modified_headers.SetHeader("Set-B", "val-b");
  b.modified_headers.SetHeader("Set-Shared", "val-shared-b");
  b.modified_cors_exempt_headers.SetHeader("Set-Cors-B", "val-cors-b");
  b.modified_cors_exempt_headers.SetHeader("Set-Cors-Shared",
                                           "val-cors-shared-b");

  a.MergeFrom(std::move(b));

  EXPECT_THAT(a.removed_headers,
              ElementsAre("Removed-A", "Removed-Shared", "Removed-B"));
  EXPECT_THAT(a.modified_headers.GetHeader("Set-A"),
              Optional(std::string("val-a")));
  EXPECT_THAT(a.modified_headers.GetHeader("Set-Shared"),
              Optional(std::string("val-shared-b")));
  EXPECT_THAT(a.modified_headers.GetHeader("Set-B"),
              Optional(std::string("val-b")));
  EXPECT_THAT(a.modified_cors_exempt_headers.GetHeader("Set-Cors-A"),
              Optional(std::string("val-cors-a")));
  EXPECT_THAT(a.modified_cors_exempt_headers.GetHeader("Set-Cors-Shared"),
              Optional(std::string("val-cors-shared-b")));
  EXPECT_THAT(a.modified_cors_exempt_headers.GetHeader("Set-Cors-B"),
              Optional(std::string("val-cors-b")));
}

constexpr char kKey1[] = "Header-1";
constexpr char kKey2[] = "Header-2";

HttpRequestHeadersUpdateParams CloneUpdateParams(
    const HttpRequestHeadersUpdateParams& params) {
  HttpRequestHeadersUpdateParams clone;
  clone.removed_headers = params.removed_headers;
  clone.modified_headers = params.modified_headers;
  clone.modified_cors_exempt_headers = params.modified_cors_exempt_headers;
  return clone;
}

// Generates the 4 combinations of headers for `kKey1` and `kKey2` (neither,
// key 1, key 2, or both).
std::vector<net::HttpRequestHeaders> GenerateAllTwoKeyHttpRequestHeaders(
    std::string_view value_prefix) {
  std::vector<net::HttpRequestHeaders> result;
  result.reserve(4);
  for (bool has_k1 : {false, true}) {
    for (bool has_k2 : {false, true}) {
      net::HttpRequestHeaders headers;
      if (has_k1) {
        headers.SetHeader(kKey1, base::StrCat({value_prefix, "-1"}));
      }
      if (has_k2) {
        headers.SetHeader(kKey2, base::StrCat({value_prefix, "-2"}));
      }
      result.push_back(std::move(headers));
    }
  }
  return result;
}

// Generates the 64 combinations of update params for `kKey1` and `kKey2`
// (4 removed combinations * 4 modified combinations * 4 cors combinations).
std::vector<HttpRequestHeadersUpdateParams>
GenerateAllTwoKeyHttpRequestHeadersUpdateParams(std::string_view value_prefix) {
  const std::vector<std::vector<std::string>> all_removed = {
      {},
      {kKey1},
      {kKey2},
      {kKey1, kKey2},
  };
  std::vector<net::HttpRequestHeaders> all_modified =
      GenerateAllTwoKeyHttpRequestHeaders(base::StrCat({value_prefix, "-mod"}));
  std::vector<net::HttpRequestHeaders> all_cors =
      GenerateAllTwoKeyHttpRequestHeaders(
          base::StrCat({value_prefix, "-cors"}));

  std::vector<HttpRequestHeadersUpdateParams> result;
  result.reserve(64);
  for (const auto& removed : all_removed) {
    for (const auto& modified : all_modified) {
      for (const auto& cors : all_cors) {
        HttpRequestHeadersUpdateParams params;
        params.removed_headers = removed;
        params.modified_headers = modified;
        params.modified_cors_exempt_headers = cors;
        result.push_back(std::move(params));
      }
    }
  }
  return result;
}

// Generates the 2 combinations of headers for `kKey1` (neither, key 1).
std::vector<net::HttpRequestHeaders> GenerateAllOneKeyHttpRequestHeaders(
    std::string_view value_prefix) {
  std::vector<net::HttpRequestHeaders> result;
  result.reserve(2);
  // Empty update.
  result.emplace_back();
  // Update of key 1.
  net::HttpRequestHeaders headers;
  headers.SetHeader(kKey1, base::StrCat({value_prefix, "-1"}));
  result.push_back(std::move(headers));

  return result;
}

// Generates the 8 combinations of update params for `kKey1`.
// (2 removed combinations * 2 modified combinations * 2 cors combinations).
std::vector<HttpRequestHeadersUpdateParams>
GenerateAllOneKeyHttpRequestHeadersUpdateParams(std::string_view value_prefix) {
  const std::vector<std::vector<std::string>> all_removed = {
      {},
      {kKey1},
  };
  std::vector<net::HttpRequestHeaders> all_modified =
      GenerateAllOneKeyHttpRequestHeaders(base::StrCat({value_prefix, "-mod"}));
  std::vector<net::HttpRequestHeaders> all_cors =
      GenerateAllOneKeyHttpRequestHeaders(
          base::StrCat({value_prefix, "-cors"}));

  std::vector<HttpRequestHeadersUpdateParams> result;
  result.reserve(8);
  for (const auto& removed : all_removed) {
    for (const auto& modified : all_modified) {
      for (const auto& cors : all_cors) {
        HttpRequestHeadersUpdateParams params;
        params.removed_headers = removed;
        params.modified_headers = modified;
        params.modified_cors_exempt_headers = cors;
        result.push_back(std::move(params));
      }
    }
  }
  return result;
}

std::string ToString(const net::HttpRequestHeaders& headers) {
  std::vector<std::string> pieces;
  for (const auto& header : headers.GetHeaderVector()) {
    pieces.push_back(base::StrCat({header.key, "=", header.value}));
  }
  return base::StrCat({"[", base::JoinString(pieces, ", "), "]"});
}

std::string ToString(const HttpRequestHeadersUpdateParams& params) {
  return base::StrCat(
      {"{removed=[", base::JoinString(params.removed_headers, ", "),
       "], modified=", ToString(params.modified_headers),
       ", cors=", ToString(params.modified_cors_exempt_headers), "}"});
}

TEST(HttpRequestHeadersUpdateParamsTest, MergeFromInChainEquivalence) {
  // Test that `a.MergeFromInChain(b); a.Apply(h, c);` is equivalent to
  // `a.Apply(h, c); b.Apply(h, c);` across all combinations of one key.
  auto test_equivalence = [](const HttpRequestHeadersUpdateParams& a,
                             const HttpRequestHeadersUpdateParams& b,
                             const net::HttpRequestHeaders& initial_headers,
                             const net::HttpRequestHeaders& initial_cors) {
    SCOPED_TRACE(testing::Message()
                 << "\ninitial_headers: " << ToString(initial_headers)
                 << "\ninitial_cors: " << ToString(initial_cors)
                 << "\na: " << ToString(a) << "\nb: " << ToString(b));

    net::HttpRequestHeaders seq_headers = initial_headers;
    net::HttpRequestHeaders seq_cors = initial_cors;
    a.Apply(seq_headers, seq_cors);
    b.Apply(seq_headers, seq_cors);

    HttpRequestHeadersUpdateParams chained = CloneUpdateParams(a);
    chained.MergeFromInChain(b);

    net::HttpRequestHeaders chain_headers = initial_headers;
    net::HttpRequestHeaders chain_cors = initial_cors;
    chained.Apply(chain_headers, chain_cors);

    EXPECT_EQ(chain_headers.ToString(), seq_headers.ToString());
    EXPECT_EQ(chain_cors.ToString(), seq_cors.ToString());
  };

  // Test all combinations of one key between the initial state and the two
  // updates (2 initial headers * 2 initial cors * (2 removed * 2 modified *
  // 2 modified cors)^2) = 256
  const auto all_initial_headers =
      GenerateAllOneKeyHttpRequestHeaders("initial");
  const auto all_initial_cors =
      GenerateAllOneKeyHttpRequestHeaders("initial-cors");
  const auto all_updates_a =
      GenerateAllOneKeyHttpRequestHeadersUpdateParams("a");
  const auto all_updates_b =
      GenerateAllOneKeyHttpRequestHeadersUpdateParams("b");
  for (const auto& initial_headers : all_initial_headers) {
    for (const auto& initial_cors : all_initial_cors) {
      for (const auto& a : all_updates_a) {
        for (const auto& b : all_updates_b) {
          test_equivalence(a, b, initial_headers, initial_cors);
        }
      }
    }
  }
}

TEST(HttpRequestHeadersUpdateParamsTest, MergeFromInChainIsCaseInsensitive) {
  HttpRequestHeadersUpdateParams a;
  a.modified_headers.SetHeader("X-A", "value-1");
  a.modified_cors_exempt_headers.SetHeader("X-B", "value-2");
  a.removed_headers = {"X-C"};
  const std::string original_headers_str = a.modified_headers.ToString();
  const std::string original_cors_exempt_str =
      a.modified_cors_exempt_headers.ToString();
  HttpRequestHeadersUpdateParams b;
  b.modified_headers.SetHeader("x-a", "value-1");
  b.modified_cors_exempt_headers.SetHeader("x-b", "value-2");
  b.removed_headers = {"x-c"};

  a.MergeFromInChain(b);

  EXPECT_EQ(a.modified_headers.ToString(), original_headers_str);
  EXPECT_EQ(a.modified_cors_exempt_headers.ToString(),
            original_cors_exempt_str);
  EXPECT_THAT(a.removed_headers, ElementsAre("X-C"));
}

TEST(HttpRequestHeadersUpdateParamsTest,
     ApplyAndReturnInverseAppliesTheUpdate) {
  // Verifies that `ApplyAndReturnInverse()` behaves in the same way as
  // `Apply()`.
  auto verify_apply = [](const HttpRequestHeadersUpdateParams& params,
                         const net::HttpRequestHeaders& initial_headers,
                         const net::HttpRequestHeaders& initial_cors) {
    SCOPED_TRACE(testing::Message()
                 << "\ninitial_headers: " << ToString(initial_headers)
                 << "\ninitial_cors: " << ToString(initial_cors)
                 << "\nparams: " << ToString(params));

    net::HttpRequestHeaders headers = initial_headers;
    net::HttpRequestHeaders cors_exempt = initial_cors;
    // Compute the result from the standard `Apply()`.
    net::HttpRequestHeaders expected_headers = initial_headers;
    net::HttpRequestHeaders expected_cors_exempt = initial_cors;
    params.Apply(expected_headers, expected_cors_exempt);

    // ApplyAndReturnInverse(net::HttpRequestHeaders&, net::HttpRequestHeaders&)
    // Ignore the return value. The inverse is tested separately.
    (void)params.ApplyAndReturnInverse(headers, cors_exempt);

    EXPECT_THAT(
        MatchHttpRequestHeaders(expected_headers, headers,
                                MatchHttpRequestHeadersValueOption::kEquals),
        IsEmpty());
    EXPECT_THAT(
        MatchHttpRequestHeaders(expected_cors_exempt, cors_exempt,
                                MatchHttpRequestHeadersValueOption::kEquals),
        IsEmpty());
  };

  // Test all combinations of two keys between the initial state and the update
  // (2^2 initial headers * 2^2 initial cors * 4 removed * 4 modified * 4
  // modified cors) = 1024
  const auto all_initial_headers =
      GenerateAllTwoKeyHttpRequestHeaders("initial");
  const auto all_initial_cors =
      GenerateAllTwoKeyHttpRequestHeaders("initial-cors");
  const auto all_updates =
      GenerateAllTwoKeyHttpRequestHeadersUpdateParams("update");
  for (const auto& initial_headers : all_initial_headers) {
    for (const auto& initial_cors : all_initial_cors) {
      for (const auto& params : all_updates) {
        verify_apply(params, initial_headers, initial_cors);
      }
    }
  }
}

TEST(HttpRequestHeadersUpdateParamsTest, InverseLeavesHeadersUnchanged) {
  // Verifies that applying the inverse (as returned from
  // `ApplyAndReturnInverse()`) leaves the headers as they were before the
  // request.
  auto verify_inverse = [](const HttpRequestHeadersUpdateParams& params,
                           const net::HttpRequestHeaders& initial_headers,
                           const net::HttpRequestHeaders& initial_cors) {
    SCOPED_TRACE(testing::Message()
                 << "\ninitial_headers: " << ToString(initial_headers)
                 << "\ninitial_cors: " << ToString(initial_cors)
                 << "\nparams: " << ToString(params));

    net::HttpRequestHeaders headers = initial_headers;
    net::HttpRequestHeaders cors_exempt = initial_cors;

    // ApplyAndReturnInverse(net::HttpRequestHeaders&, net::HttpRequestHeaders&)
    HttpRequestHeadersUpdateParams inverse =
        params.ApplyAndReturnInverse(headers, cors_exempt);
    inverse.Apply(headers, cors_exempt);

    EXPECT_THAT(
        MatchHttpRequestHeaders(initial_headers, headers,
                                MatchHttpRequestHeadersValueOption::kEquals),
        IsEmpty());
    EXPECT_THAT(
        MatchHttpRequestHeaders(initial_cors, cors_exempt,
                                MatchHttpRequestHeadersValueOption::kEquals),
        IsEmpty());
  };

  // Test all combinations of two keys between the initial state and the update
  // (2^2 initial headers * 2^2 initial cors * 4 removed * 4 modified * 4
  // modified cors) = 1024
  const auto all_initial_headers =
      GenerateAllTwoKeyHttpRequestHeaders("initial");
  const auto all_initial_cors =
      GenerateAllTwoKeyHttpRequestHeaders("initial-cors");
  const auto all_updates =
      GenerateAllTwoKeyHttpRequestHeadersUpdateParams("update");
  for (const auto& initial_headers : all_initial_headers) {
    for (const auto& initial_cors : all_initial_cors) {
      for (const auto& params : all_updates) {
        verify_inverse(params, initial_headers, initial_cors);
      }
    }
  }
}

TEST(HttpRequestHeadersUpdateParamsTest, SetHeader) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Header-A", "initial-a");
  headers.SetHeader("Header-B", "initial-b");
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader("Header-A", "initial-cors-a");

  HttpRequestHeadersUpdateParams params;
  params.SetHeader("Header-A", "value-a");
  params.Apply(headers, cors_exempt_headers);

  EXPECT_THAT(headers.GetHeader("Header-A"), Optional(std::string("value-a")));
  EXPECT_THAT(headers.GetHeader("Header-B"),
              Optional(std::string("initial-b")));
  EXPECT_THAT(cors_exempt_headers.GetHeader("Header-A"),
              Optional(std::string("initial-cors-a")));
}

TEST(HttpRequestHeadersUpdateParamsTest,
     SetHeader_DoesNotRevertRemovalOfCorsExemptHeader) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Header-A", "initial-a");
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader("Header-A", "initial-cors-a");

  HttpRequestHeadersUpdateParams params;
  params.removed_headers.push_back("Header-A");
  params.SetHeader("Header-A", "value-a");
  params.Apply(headers, cors_exempt_headers);

  EXPECT_THAT(headers.GetHeader("Header-A"), Optional(std::string("value-a")));
  EXPECT_FALSE(cors_exempt_headers.HasHeader("Header-A"));
}

TEST(HttpRequestHeadersUpdateParamsTest, SetCorsExemptHeader) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Header-A", "initial-a");
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader("Cors-A", "initial-cors-a");
  cors_exempt_headers.SetHeader("Cors-B", "initial-cors-b");

  HttpRequestHeadersUpdateParams params;
  params.SetCorsExemptHeader("Cors-A", "cors-value-a");
  params.Apply(headers, cors_exempt_headers);

  EXPECT_THAT(headers.GetHeader("Header-A"),
              Optional(std::string("initial-a")));
  EXPECT_FALSE(headers.HasHeader("Cors-A"));
  EXPECT_THAT(cors_exempt_headers.GetHeader("Cors-A"),
              Optional(std::string("cors-value-a")));
  EXPECT_THAT(cors_exempt_headers.GetHeader("Cors-B"),
              Optional(std::string("initial-cors-b")));
}

TEST(HttpRequestHeadersUpdateParamsTest,
     SetCorsExemptHeader_DoesNotRevertRemovalOfHeader) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Header-A", "initial-a");
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader("Header-A", "initial-cors-a");

  HttpRequestHeadersUpdateParams params;
  params.removed_headers.push_back("Header-A");
  params.SetCorsExemptHeader("Header-A", "value-cors-a");
  params.Apply(headers, cors_exempt_headers);

  EXPECT_THAT(cors_exempt_headers.GetHeader("Header-A"),
              Optional(std::string("value-cors-a")));
  EXPECT_FALSE(headers.HasHeader("Header-A"));
}

TEST(HttpRequestHeadersUpdateParamsTest,
     RemoveHeader_DoesNotAddDuplicatesRegardlessOfCasing) {
  HttpRequestHeadersUpdateParams params;
  params.removed_headers.push_back("Header-A");

  // Calling RemoveHeader multiple times or with different casing does not
  // duplicate the entry in `removed_headers`.
  params.RemoveHeader("Header-A");
  params.RemoveHeader("header-a");
  params.RemoveHeader("HEADER-A");
  params.RemoveHeader("hEaDeR-A");

  EXPECT_THAT(params.removed_headers, ElementsAre("Header-A"));
}

TEST(HttpRequestHeadersUpdateParamsTest, RemoveHeader) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Delete-Shared-A", "old-a");
  headers.SetHeader("Delete-Header-B", "old-b");
  headers.SetHeader("Keep-Header-C", "old-c");
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader("Delete-Shared-A", "cors-a");
  cors_exempt_headers.SetHeader("Delete-Cors-D", "cors-d");
  cors_exempt_headers.SetHeader("Keep-Cors-E", "cors-e");

  HttpRequestHeadersUpdateParams params;
  params.RemoveHeader("Delete-Shared-A");
  params.RemoveHeader("Delete-Header-B");
  params.RemoveHeader("Delete-Cors-D");
  params.Apply(headers, cors_exempt_headers);

  EXPECT_FALSE(headers.HasHeader("Delete-Shared-A"));
  EXPECT_FALSE(headers.HasHeader("Delete-Header-B"));
  EXPECT_THAT(headers.GetHeader("Keep-Header-C"),
              Optional(std::string("old-c")));
  EXPECT_FALSE(cors_exempt_headers.HasHeader("Delete-Shared-A"));
  EXPECT_FALSE(cors_exempt_headers.HasHeader("Delete-Cors-D"));
  EXPECT_THAT(cors_exempt_headers.GetHeader("Keep-Cors-E"),
              Optional(std::string("cors-e")));
}

TEST(HttpRequestHeadersUpdateParamsTest, SetHeaderThenRemoveHeader) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Header-A", "old-a");
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader("Cors-A", "cors-a");

  HttpRequestHeadersUpdateParams params;
  params.SetHeader("Header-A", "new-a");
  params.SetCorsExemptHeader("Cors-A", "new-cors-a");
  params.RemoveHeader("Header-A");
  params.RemoveHeader("Cors-A");
  params.Apply(headers, cors_exempt_headers);

  EXPECT_FALSE(headers.HasHeader("Header-A"));
  EXPECT_FALSE(cors_exempt_headers.HasHeader("Cors-A"));
}

TEST(HttpRequestHeadersUpdateParamsTest, RemoveHeaderThenSetHeader) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Header-A", "old-a");
  headers.SetHeader("Shared-A", "shared-a");
  net::HttpRequestHeaders cors_exempt_headers;
  cors_exempt_headers.SetHeader("Cors-A", "cors-a");
  cors_exempt_headers.SetHeader("Shared-A", "shared-cors-a");

  HttpRequestHeadersUpdateParams params;
  params.RemoveHeader("Header-A");
  params.RemoveHeader("Cors-A");
  params.RemoveHeader("Shared-A");
  params.SetHeader("Header-A", "new-a");
  params.SetCorsExemptHeader("Cors-A", "new-cors-a");
  params.Apply(headers, cors_exempt_headers);

  EXPECT_THAT(headers.GetHeader("Header-A"), Optional(std::string("new-a")));
  EXPECT_FALSE(headers.HasHeader("Shared-A"));
  EXPECT_THAT(cors_exempt_headers.GetHeader("Cors-A"),
              Optional(std::string("new-cors-a")));
  EXPECT_FALSE(cors_exempt_headers.HasHeader("Shared-A"));
}

}  // namespace
}  // namespace network
