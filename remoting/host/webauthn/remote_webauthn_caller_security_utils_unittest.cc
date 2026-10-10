// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_caller_security_utils.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using StringType = base::CommandLine::StringType;

constexpr base::CommandLine::CharType kProdSecurityKeyOrigin[] =
    FILE_PATH_LITERAL("chrome-extension://djjmngfglakhkhmgcfdmjalogilepkhd/");

struct OriginTestParam {
  const char* name;
  const base::CommandLine::CharType* origin;
};

// Returns whether the origin check passes for a command line consisting of a
// program name followed by `args`.
bool IsTrustedExtensionWithArgs(std::vector<StringType> args) {
  args.insert(args.begin(), FILE_PATH_LITERAL("remote_webauthn"));
  return IsLaunchedByTrustedExtension(base::CommandLine(args));
}

std::string GetOriginTestName(
    const testing::TestParamInfo<OriginTestParam>& info) {
  return info.param.name;
}

}  // namespace

class RemoteWebAuthnAcceptedOriginTest
    : public testing::TestWithParam<OriginTestParam> {};

TEST_P(RemoteWebAuthnAcceptedOriginTest, IsAccepted) {
  EXPECT_TRUE(IsTrustedExtensionWithArgs({GetParam().origin}));
}

INSTANTIATE_TEST_SUITE_P(
    IsLaunchedByTrustedExtensionTest,
    RemoteWebAuthnAcceptedOriginTest,
    testing::Values(
        OriginTestParam{"ProdSecurityKey", kProdSecurityKeyOrigin},
        OriginTestParam{
            "ProdSecurityKeyWithoutTrailingSlash",
            FILE_PATH_LITERAL(
                "chrome-extension://djjmngfglakhkhmgcfdmjalogilepkhd")},
        OriginTestParam{
            "ProdCompanion",
            FILE_PATH_LITERAL(
                "chrome-extension://inomeogfingihgjfjlpeplalcfajhgai/")},
        OriginTestParam{
            "ProdCompanionWithoutTrailingSlash",
            FILE_PATH_LITERAL(
                "chrome-extension://inomeogfingihgjfjlpeplalcfajhgai")}
#if !defined(NDEBUG)
        ,
        OriginTestParam{
            "DevSecurityKey",
            FILE_PATH_LITERAL(
                "chrome-extension://kbapnajlciffffomeaphfpckfdcfopef/")},
        OriginTestParam{
            "DevCompanion",
            FILE_PATH_LITERAL(
                "chrome-extension://pbnaomcgbfiofkfobmlhmdobjchjkphi/")}
#endif
        ),
    &GetOriginTestName);

class RemoteWebAuthnRejectedOriginTest
    : public testing::TestWithParam<OriginTestParam> {};

TEST_P(RemoteWebAuthnRejectedOriginTest, IsRejected) {
  EXPECT_FALSE(IsTrustedExtensionWithArgs({GetParam().origin}));
}

INSTANTIATE_TEST_SUITE_P(
    IsLaunchedByTrustedExtensionTest,
    RemoteWebAuthnRejectedOriginTest,
    testing::Values(
        OriginTestParam{"Empty", FILE_PATH_LITERAL("")},
        OriginTestParam{
            "UnknownExtension",
            FILE_PATH_LITERAL(
                "chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/")},
        OriginTestParam{
            "SuffixAppendedToId",
            FILE_PATH_LITERAL(
                "chrome-extension://djjmngfglakhkhmgcfdmjalogilepkhd.evil/")},
        OriginTestParam{
            "PathAfterTrailingSlash",
            FILE_PATH_LITERAL(
                "chrome-extension://djjmngfglakhkhmgcfdmjalogilepkhd/foo")},
        OriginTestParam{
            "DoubleTrailingSlash",
            FILE_PATH_LITERAL(
                "chrome-extension://djjmngfglakhkhmgcfdmjalogilepkhd//")},
        OriginTestParam{"PrefixOnly", FILE_PATH_LITERAL("chrome-extension://")},
        OriginTestParam{"PrefixAndSlashOnly",
                        FILE_PATH_LITERAL("chrome-extension:///")},
        OriginTestParam{
            "HttpsScheme",
            FILE_PATH_LITERAL("https://djjmngfglakhkhmgcfdmjalogilepkhd/")},
        OriginTestParam{
            "UppercaseScheme",
            FILE_PATH_LITERAL(
                "CHROME-EXTENSION://djjmngfglakhkhmgcfdmjalogilepkhd/")},
        OriginTestParam{"NoScheme",
                        FILE_PATH_LITERAL("djjmngfglakhkhmgcfdmjalogilepkhd")},
        OriginTestParam{
            "UppercaseId",
            FILE_PATH_LITERAL(
                "chrome-extension://DJJMNGFGLAKHKHMGCFDMJALOGILEPKHD/")}
#if defined(NDEBUG)
        ,
        OriginTestParam{
            "DevSecurityKeyInReleaseBuild",
            FILE_PATH_LITERAL(
                "chrome-extension://kbapnajlciffffomeaphfpckfdcfopef/")},
        OriginTestParam{
            "DevCompanionInReleaseBuild",
            FILE_PATH_LITERAL(
                "chrome-extension://pbnaomcgbfiofkfobmlhmdobjchjkphi/")}
#endif
        ),
    &GetOriginTestName);

TEST(IsLaunchedByTrustedExtensionTest, NoArgs_ReturnsFalse) {
  EXPECT_FALSE(IsTrustedExtensionWithArgs({}));
}

TEST(IsLaunchedByTrustedExtensionTest, OnlySwitches_ReturnsFalse) {
  EXPECT_FALSE(
      IsTrustedExtensionWithArgs({FILE_PATH_LITERAL("--parent-window=123")}));
}

TEST(IsLaunchedByTrustedExtensionTest,
     AllowedOriginWithParentWindowSwitch_ReturnsTrue) {
  EXPECT_TRUE(IsTrustedExtensionWithArgs(
      {kProdSecurityKeyOrigin, FILE_PATH_LITERAL("--parent-window=123")}));
}

// base::CommandLine separates switches from positional arguments, so the origin
// is still the first positional argument even if a switch precedes it. Chrome
// always passes the origin first; the launcher itself is verified by
// IsLaunchedByTrustedProcess().
TEST(IsLaunchedByTrustedExtensionTest, AllowedOriginAfterSwitch_ReturnsTrue) {
  EXPECT_TRUE(IsTrustedExtensionWithArgs(
      {FILE_PATH_LITERAL("--parent-window=123"), kProdSecurityKeyOrigin}));
}

TEST(IsLaunchedByTrustedExtensionTest, AllowedOriginNotFirst_ReturnsFalse) {
  EXPECT_FALSE(IsTrustedExtensionWithArgs(
      {FILE_PATH_LITERAL("foo"), kProdSecurityKeyOrigin}));
}

}  // namespace remoting
