// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/common/manifest_id.h"

#include <optional>
#include <sstream>
#include <utility>

#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace webapps {

TEST(ValidManifestIdTest, Hashing) {
  auto id1 = ValidManifestId::Create("https://example.com/app");
  auto id2 = ValidManifestId::Create("https://example.com/app");
  auto id3 = ValidManifestId::Create("https://other.com/app");

  ASSERT_TRUE(id1.has_value());
  ASSERT_TRUE(id2.has_value());
  ASSERT_TRUE(id3.has_value());

  absl::flat_hash_set<ValidManifestId> set;
  set.insert(*id1);
  EXPECT_TRUE(set.contains(*id2));
  EXPECT_FALSE(set.contains(*id3));
}

TEST(ValidManifestIdTest, CreateFromString) {
  // Valid URL
  auto id1 = ValidManifestId::Create("https://example.com/app");
  ASSERT_TRUE(id1.has_value());
  EXPECT_TRUE(id1->value().is_valid());
  EXPECT_EQ(id1->value().spec(), "https://example.com/app");

  // Invalid URL
  auto id2 = ValidManifestId::Create("not_a_url");
  EXPECT_FALSE(id2.has_value());

  // Empty string
  auto id3 = ValidManifestId::Create("");
  EXPECT_FALSE(id3.has_value());
}

TEST(ValidManifestIdTest, CreateFromGURL) {
  // Valid GURL
  GURL url("https://example.com/start");
  auto id1 = ValidManifestId::Create(url);
  ASSERT_TRUE(id1.has_value());
  EXPECT_EQ(id1->value().spec(), url);

  // Invalid GURL
  GURL invalid_url;
  auto id2 = ValidManifestId::Create(invalid_url);
  EXPECT_FALSE(id2.has_value());

  // Invalid String
  GURL invalid("str_invalid");
  auto id3 = ValidManifestId::Create(invalid);
  EXPECT_FALSE(id3.has_value());
}

TEST(ValidManifestIdTest, Constructors) {
  GURL url("https://example.com/");
  ValidManifestId id1(url);
  EXPECT_EQ(id1.value(), url);

  ValidManifestId id2 = id1;
  EXPECT_EQ(id2.value(), url);
}

TEST(ValidManifestIdTest, Spec) {
  auto id_opt = ValidManifestId::Create("https://example.com/app");
  ASSERT_TRUE(id_opt.has_value());
  ValidManifestId id = *id_opt;
  EXPECT_EQ(id.spec(), "https://example.com/app");
}

TEST(ValidManifestIdTest, StripsRef) {
  // From string
  auto id1 = ValidManifestId::Create("https://example.com/app.html#ref");
  ASSERT_TRUE(id1.has_value());
  EXPECT_EQ(id1->spec(), "https://example.com/app.html");

  // From GURL
  GURL url_with_ref("https://example.com/app.html#ref");
  auto id2 = ValidManifestId::Create(url_with_ref);
  ASSERT_TRUE(id2.has_value());
  EXPECT_EQ(id2->spec(), "https://example.com/app.html");
}

TEST(ValidManifestIdTest, Operators) {
  auto opt_id1 = ValidManifestId::Create("https://a.com/");
  auto opt_id2 = ValidManifestId::Create("https://a.com/");
  auto opt_id3 = ValidManifestId::Create("https://b.com/");

  ASSERT_TRUE(opt_id1.has_value());
  ASSERT_TRUE(opt_id2.has_value());
  ASSERT_TRUE(opt_id3.has_value());

  ValidManifestId id1 = *opt_id1;
  ValidManifestId id2 = *opt_id2;
  ValidManifestId id3 = *opt_id3;

  EXPECT_EQ(id1, id2);
  EXPECT_NE(id1, id3);

  // Stream operator
  std::stringstream ss;
  ss << id1;
  EXPECT_EQ(ss.str(), "https://a.com/");
}

TEST(ValidManifestIdTest, CompareWithGURL) {
  std::string url_str = "https://example.com/app";
  std::string different_url_str = "https://other.com/app";

  auto id_opt = ValidManifestId::Create(url_str);
  ASSERT_TRUE(id_opt.has_value());
  ValidManifestId id = *id_opt;

  GURL url(url_str);
  GURL different_url(different_url_str);

  // Compare with identical GURL
  EXPECT_EQ(id, url);
  EXPECT_EQ(url, id);
  EXPECT_FALSE(id != url);
  EXPECT_FALSE(url != id);

  // Compare with GURL that has a ref
  GURL url_with_ref(url_str + "#ref");
  EXPECT_EQ(id, url_with_ref);
  EXPECT_EQ(url_with_ref, id);
  EXPECT_FALSE(id != url_with_ref);
  EXPECT_FALSE(url_with_ref != id);

  // Compare with different GURL
  EXPECT_NE(id, different_url);
  EXPECT_NE(different_url, id);
  EXPECT_TRUE(id != different_url);
  EXPECT_TRUE(different_url != id);
}

}  // namespace webapps