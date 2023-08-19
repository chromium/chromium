// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

TEST(ParsedPermissionsPolicyDeclarationTest, Contains) {
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.test/"));
  const url::Origin kOpaqueOrigin = url::Origin();

  // Empty / default declaration.
  ParsedPermissionsPolicyDeclaration empty_decl;
  EXPECT_FALSE(empty_decl.Contains(kTestOrigin));
  EXPECT_FALSE(empty_decl.Contains(kOpaqueOrigin));

  // Matches opaque.
  ParsedPermissionsPolicyDeclaration opaque_decl;
  opaque_decl.matches_opaque_src = true;
  EXPECT_FALSE(opaque_decl.Contains(kTestOrigin));
  EXPECT_TRUE(opaque_decl.Contains(kOpaqueOrigin));

  // Matches all.
  ParsedPermissionsPolicyDeclaration all_decl;
  all_decl.matches_all_origins = true;
  EXPECT_TRUE(all_decl.Contains(kTestOrigin));
  EXPECT_TRUE(all_decl.Contains(kOpaqueOrigin));

  // Origin mismatch.
  ParsedPermissionsPolicyDeclaration mismatch_decl;
  mismatch_decl.allowed_origins.emplace_back(
      *OriginWithPossibleWildcards::FromOrigin(
          url::Origin::Create(GURL("https://example2.test/"))));
  EXPECT_FALSE(mismatch_decl.Contains(kTestOrigin));
  EXPECT_FALSE(mismatch_decl.Contains(kOpaqueOrigin));

  // Origin match.
  ParsedPermissionsPolicyDeclaration match_decl;
  match_decl.allowed_origins.emplace_back(
      *OriginWithPossibleWildcards::FromOrigin(
          url::Origin::Create(GURL("https://example.test/"))));
  EXPECT_TRUE(match_decl.Contains(kTestOrigin));
  EXPECT_FALSE(match_decl.Contains(kOpaqueOrigin));

  // Self match.
  ParsedPermissionsPolicyDeclaration self_decl;
  self_decl.self_if_matches =
      url::Origin::Create(GURL("https://example.test/"));
  EXPECT_TRUE(self_decl.Contains(kTestOrigin));
  EXPECT_FALSE(self_decl.Contains(kOpaqueOrigin));

  // Opaque self match.
  ParsedPermissionsPolicyDeclaration opaque_self_decl;
  opaque_self_decl.self_if_matches = kOpaqueOrigin;
  EXPECT_FALSE(opaque_self_decl.Contains(kTestOrigin));
  EXPECT_TRUE(opaque_self_decl.Contains(kOpaqueOrigin));
}

}  // namespace blink
