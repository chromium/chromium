// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/variable_dictionary.h"

#include <utility>

#include "base/location.h"
#include "base/strings/string_piece.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/test_util.h"
#include "media/formats/hls/types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media::hls {

namespace {

VariableDictionary CreateBasicDictionary(
    const base::Location& from = base::Location::Current()) {
  VariableDictionary dict;
  EXPECT_TRUE(dict.Insert(CreateVarName("NAME"), "bond")) << from.ToString();
  EXPECT_TRUE(dict.Insert(CreateVarName("IDENT"), "007")) << from.ToString();
  EXPECT_TRUE(dict.Insert(CreateVarName("_0THER-1dent"), "{$james}"))
      << from.ToString();

  return dict;
}

void OkTest(const VariableDictionary& dict,
            base::StringPiece in,
            base::StringPiece expected_out,
            const base::Location& from = base::Location::Current()) {
  const auto source_str = SourceString::CreateForTesting(in);
  VariableDictionary::SubstitutionBuffer buffer;
  auto result = dict.Resolve(source_str, buffer);
  ASSERT_TRUE(result.has_value()) << from.ToString();
  EXPECT_EQ(std::move(result).value(), expected_out) << from.ToString();
}

void ErrorTest(const VariableDictionary& dict,
               base::StringPiece in,
               ParseStatusCode expected_error,
               const base::Location& from = base::Location::Current()) {
  const auto source_str = SourceString::CreateForTesting(in);
  VariableDictionary::SubstitutionBuffer buffer;
  auto result = dict.Resolve(source_str, buffer);
  ASSERT_TRUE(result.has_error()) << from.ToString();
  EXPECT_EQ(std::move(result).error(), expected_error) << from.ToString();
}

}  // namespace

TEST(HlsVariableDictionaryTest, BasicSubstitution) {
  VariableDictionary dict = CreateBasicDictionary();
  OkTest(dict, "The NAME's {$NAME}, {$_0THER-1dent} {$NAME}. Agent {$IDENT}",
         "The NAME's bond, {$james} bond. Agent 007");
  OkTest(dict, "This $tring {has} ${no} v{}{}ar}}s",
         "This $tring {has} ${no} v{}{}ar}}s");
}

TEST(HlsVariableDictionaryTest, VariableUndefined) {
  VariableDictionary dict;

  // Names are case-sensitive
  EXPECT_TRUE(dict.Insert(CreateVarName("TEST"), "FOO"));
  EXPECT_EQ(dict.Find(CreateVarName("TEST")),
            absl::make_optional<base::StringPiece>("FOO"));
  EXPECT_EQ(dict.Find(CreateVarName("test")), absl::nullopt);

  ErrorTest(dict, "Hello {$test}", ParseStatusCode::kVariableUndefined);
  OkTest(dict, "Hello {$TEST}", "Hello FOO");
  ErrorTest(dict, "Hello {$TEST} {$TEST1}",
            ParseStatusCode::kVariableUndefined);
}

TEST(HlsVariableDictionaryTest, RedefinitionNotAllowed) {
  VariableDictionary dict;
  EXPECT_TRUE(dict.Insert(CreateVarName("TEST"), "FOO"));
  EXPECT_EQ(dict.Find(CreateVarName("TEST")),
            absl::make_optional<base::StringPiece>("FOO"));

  // Redefinition of a variable is not allowed, with the same or different value
  EXPECT_FALSE(dict.Insert(CreateVarName("TEST"), "FOO"));
  EXPECT_FALSE(dict.Insert(CreateVarName("TEST"), "BAR"));
  EXPECT_EQ(dict.Find(CreateVarName("TEST")),
            absl::make_optional<base::StringPiece>("FOO"));

  // Variable names are case-sensitive
  EXPECT_TRUE(dict.Insert(CreateVarName("TEsT"), "BAR"));
  EXPECT_EQ(dict.Find(CreateVarName("TEsT")),
            absl::make_optional<base::StringPiece>("BAR"));
  EXPECT_EQ(dict.Find(CreateVarName("TEST")),
            absl::make_optional<base::StringPiece>("FOO"));

  EXPECT_TRUE(dict.Insert(CreateVarName("TEST2"), "BAZ"));
  EXPECT_EQ(dict.Find(CreateVarName("TEST2")),
            absl::make_optional<base::StringPiece>("BAZ"));
  EXPECT_EQ(dict.Find(CreateVarName("TEsT")),
            absl::make_optional<base::StringPiece>("BAR"));
  EXPECT_EQ(dict.Find(CreateVarName("TEST")),
            absl::make_optional<base::StringPiece>("FOO"));
}

TEST(HlsVariableDictionaryTest, IgnoreInvalidRefSequence) {
  auto dict = CreateBasicDictionary();

  // Variable refs with invalid variable names are ignored
  OkTest(dict, "http://{$}.com", "http://{$}.com");
  OkTest(dict, "http://{$ NAME}.com", "http://{$ NAME}.com");
  OkTest(dict, "http://{$NAME }.com", "http://{$NAME }.com");
  OkTest(dict, "http://{$:NAME}.com", "http://{$:NAME}.com");

  // Incomplete variable ref sequences are ignored
  OkTest(dict, "http://{$NAME", "http://{$NAME");
  OkTest(dict, "http://{NAME}.com", "http://{NAME}.com");
  OkTest(dict, "http://${NAME}.com", "http://${NAME}.com");
  OkTest(dict, "http://$NAME.com", "http://$NAME.com");

  // Valid ref sequences surrounded by invalid ref sequences should *not* be
  // ignored
  OkTest(dict, "http://{$}{$ NAME}{$NAME}}{$NAME }.com",
         "http://{$}{$ NAME}bond}{$NAME }.com");

  // Valid ref sequences nested within invalid ref sequences should *not* be
  // ignored
  OkTest(dict, "http://{$ {$NAME}}.com", "http://{$ bond}.com");
}

TEST(HlsVariableDictionaryTest, ExplosiveVariableDefs) {
  // Variable substitution is by design not recursive
  VariableDictionary dict;
  EXPECT_TRUE(dict.Insert(CreateVarName("LOL1"), "LOLLOLLOL"));
  EXPECT_TRUE(dict.Insert(CreateVarName("LOL2"), "{$LOL1}{$LOL1}{$LOL1}"));
  EXPECT_TRUE(dict.Insert(CreateVarName("LOL3"), "{$LOL2}{$LOL2}{$LOL2}"));
  OkTest(dict, "{$LOL3}{$LOL3}{$LOL3}",
         "{$LOL2}{$LOL2}{$LOL2}{$LOL2}{$LOL2}{$LOL2}{$LOL2}{$LOL2}{$LOL2}");

  // Variable substitution is by design not cyclical
  EXPECT_TRUE(dict.Insert(CreateVarName("CYCLE"), "{$CYCLE}"));
  OkTest(dict, "{$CYCLE}", "{$CYCLE}");
}

}  // namespace media::hls
