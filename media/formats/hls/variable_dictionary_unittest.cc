// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/variable_dictionary.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/location.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/test_util.h"
#include "media/formats/hls/types.h"
#include "testing/gtest/include/gtest/gtest.h"

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
            std::string_view in,
            std::string_view expected_out,
            bool substitutions_expected,
            const base::Location& from = base::Location::Current()) {
  const auto source_str = SourceString::CreateForTesting(in);
  EXPECT_FALSE(source_str.ContainsSubstitutions()) << from.ToString();
  VariableDictionary::SubstitutionBuffer buffer;
  auto result = dict.Resolve(source_str, buffer);
  ASSERT_TRUE(result.has_value()) << from.ToString();
  auto result_str = std::move(result).value();
  EXPECT_EQ(result_str.Str(), expected_out) << from.ToString();
  EXPECT_EQ(result_str.ContainsSubstitutions(), substitutions_expected)
      << from.ToString();
}

void ErrorTest(const VariableDictionary& dict,
               std::string_view in,
               ParseStatusCode expected_error,
               const base::Location& from = base::Location::Current()) {
  const auto source_str = SourceString::CreateForTesting(in);
  VariableDictionary::SubstitutionBuffer buffer;
  auto result = dict.Resolve(source_str, buffer);
  ASSERT_FALSE(result.has_value()) << from.ToString();
  EXPECT_EQ(std::move(result).error(), expected_error) << from.ToString();
}

// Helper for cases where no substitutions should occur
void NopTest(const VariableDictionary& dict,
             std::string_view in,
             const base::Location& from = base::Location::Current()) {
  OkTest(dict, in, in, false, from);
}

}  // namespace

TEST(HlsVariableDictionaryTest, BasicSubstitution) {
  VariableDictionary dict = CreateBasicDictionary();
  OkTest(dict, "The NAME's {$NAME}, {$_0THER-1dent} {$NAME}. Agent {$IDENT}",
         "The NAME's bond, {$james} bond. Agent 007", true);
  NopTest(dict, "This $tring {has} ${no} v{}{}ar}}s");
}

TEST(HlsVariableDictionaryTest, VariableUndefined) {
  VariableDictionary dict;

  // Names are case-sensitive
  EXPECT_TRUE(dict.Insert(CreateVarName("TEST"), "FOO"));
  EXPECT_EQ(dict.Find(CreateVarName("TEST")),
            std::make_optional<std::string_view>("FOO"));
  EXPECT_EQ(dict.Find(CreateVarName("test")), std::nullopt);

  ErrorTest(dict, "Hello {$test}", ParseStatusCode::kVariableUndefined);
  OkTest(dict, "Hello {$TEST}", "Hello FOO", true);
  ErrorTest(dict, "Hello {$TEST} {$TEST1}",
            ParseStatusCode::kVariableUndefined);
}

TEST(HlsVariableDictionaryTest, RedefinitionNotAllowed) {
  VariableDictionary dict;
  EXPECT_TRUE(dict.Insert(CreateVarName("TEST"), "FOO"));
  EXPECT_EQ(dict.Find(CreateVarName("TEST")),
            std::make_optional<std::string_view>("FOO"));

  // Redefinition of a variable is not allowed, with the same or different value
  EXPECT_FALSE(dict.Insert(CreateVarName("TEST"), "FOO"));
  EXPECT_FALSE(dict.Insert(CreateVarName("TEST"), "BAR"));
  EXPECT_EQ(dict.Find(CreateVarName("TEST")),
            std::make_optional<std::string_view>("FOO"));

  // Variable names are case-sensitive
  EXPECT_TRUE(dict.Insert(CreateVarName("TEsT"), "BAR"));
  EXPECT_EQ(dict.Find(CreateVarName("TEsT")),
            std::make_optional<std::string_view>("BAR"));
  EXPECT_EQ(dict.Find(CreateVarName("TEST")),
            std::make_optional<std::string_view>("FOO"));

  EXPECT_TRUE(dict.Insert(CreateVarName("TEST2"), "BAZ"));
  EXPECT_EQ(dict.Find(CreateVarName("TEST2")),
            std::make_optional<std::string_view>("BAZ"));
  EXPECT_EQ(dict.Find(CreateVarName("TEsT")),
            std::make_optional<std::string_view>("BAR"));
  EXPECT_EQ(dict.Find(CreateVarName("TEST")),
            std::make_optional<std::string_view>("FOO"));
}

TEST(HlsVariableDictionaryTest, IgnoreInvalidRefSequence) {
  auto dict = CreateBasicDictionary();

  // Variable refs with invalid variable names are ignored
  NopTest(dict, "http://{$}.com");
  NopTest(dict, "http://{$ NAME}.com");
  NopTest(dict, "http://{$NAME }.com");
  NopTest(dict, "http://{$:NAME}.com");

  // Incomplete variable ref sequences are ignored
  NopTest(dict, "http://{$NAME");
  NopTest(dict, "http://{NAME}.com");
  NopTest(dict, "http://${NAME}.com");
  NopTest(dict, "http://$NAME.com");

  // Valid ref sequences surrounded by invalid ref sequences should *not* be
  // ignored
  OkTest(dict, "http://{$}{$ NAME}{$NAME}}{$NAME }.com",
         "http://{$}{$ NAME}bond}{$NAME }.com", true);

  // Valid ref sequences nested within invalid ref sequences should *not* be
  // ignored
  OkTest(dict, "http://{$ {$NAME}}.com", "http://{$ bond}.com", true);
}

TEST(HlsVariableDictionaryTest, ExplosiveVariableDefs) {
  // Variable substitution is by design not recursive
  VariableDictionary dict;
  EXPECT_TRUE(dict.Insert(CreateVarName("LOL1"), "LOLLOLLOL"));
  EXPECT_TRUE(dict.Insert(CreateVarName("LOL2"), "{$LOL1}{$LOL1}{$LOL1}"));
  EXPECT_TRUE(dict.Insert(CreateVarName("LOL3"), "{$LOL2}{$LOL2}{$LOL2}"));
  OkTest(dict, "{$LOL3}{$LOL3}{$LOL3}",
         "{$LOL2}{$LOL2}{$LOL2}{$LOL2}{$LOL2}{$LOL2}{$LOL2}{$LOL2}{$LOL2}",
         true);

  // Variable substitution is by design not cyclical
  EXPECT_TRUE(dict.Insert(CreateVarName("CYCLE"), "{$CYCLE}"));
  OkTest(dict, "{$CYCLE}", "{$CYCLE}", true);
}

}  // namespace media::hls
