// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(WebSandboxFlagsTest, All) {
  using mojom::WebSandboxFlags;
  const struct {
    const std::string input;
    const WebSandboxFlagsParsingResult expected_output;
  } kTestCases[] = {
      // The empty policy:
      {"", {WebSandboxFlags::kAll, ""}},

      // Every supported token:
      {"allow-downloads", {~WebSandboxFlags::kDownloads, ""}},
      {"allow-forms", {~WebSandboxFlags::kForms, ""}},
      {"allow-modals", {~WebSandboxFlags::kModals, ""}},
      {"allow-orientation-lock", {~WebSandboxFlags::kOrientationLock, ""}},
      {"allow-pointer-lock", {~WebSandboxFlags::kPointerLock, ""}},
      {"allow-popups",
       {~WebSandboxFlags::kPopups &
            ~WebSandboxFlags::kTopNavigationToCustomProtocols,
        ""}},
      {"allow-popups-to-escape-sandbox",
       {~WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts, ""}},
      {"allow-presentation", {~WebSandboxFlags::kPresentationController, ""}},
      {"allow-same-origin", {~WebSandboxFlags::kOrigin, ""}},
      {"allow-scripts",
       {~WebSandboxFlags::kAutomaticFeatures & ~WebSandboxFlags::kScripts, ""}},
      {"allow-storage-access-by-user-activation",
       {~WebSandboxFlags::kStorageAccessByUserActivation, ""}},
      {"allow-top-navigation",
       {~WebSandboxFlags::kTopNavigation &
            ~WebSandboxFlags::kTopNavigationToCustomProtocols,
        ""}},
      {"allow-top-navigation-by-user-activation",
       {~WebSandboxFlags::kTopNavigationByUserActivation, ""}},
      {"allow-top-navigation-to-custom-protocols",
       {~WebSandboxFlags::kTopNavigationToCustomProtocols, ""}},

      // Two tokens:
      {"allow-downloads allow-forms",
       {~WebSandboxFlags::kDownloads & ~WebSandboxFlags::kForms, ""}},

      // Tokens are split using https://infra.spec.whatwg.org/#ascii-whitespace
      // This is different from: base::kWhitespaceASCII.
      {"allow-downloads\nallow-forms",
       {~WebSandboxFlags::kDownloads & ~WebSandboxFlags::kForms, ""}},
      {"allow-downloads\tallow-forms",
       {~WebSandboxFlags::kDownloads & ~WebSandboxFlags::kForms, ""}},
      {"allow-downloads\rallow-forms",
       {~WebSandboxFlags::kDownloads & ~WebSandboxFlags::kForms, ""}},
      {"allow-downloads\vallow-forms",
       {WebSandboxFlags::kAll,
        "'allow-downloads\vallow-forms' is an invalid sandbox flag."}},

      // The parser is not case sensitive:
      {"ALLOW-DOWNLOADS", {~WebSandboxFlags::kDownloads, ""}},
      {"AlLoW-DoWnLoAdS", {~WebSandboxFlags::kDownloads, ""}},

      // Additional spaces are ignored:
      {" allow-downloads", {~WebSandboxFlags::kDownloads, ""}},
      {"allow-downloads ", {~WebSandboxFlags::kDownloads, ""}},
      {" allow-downloads ", {~WebSandboxFlags::kDownloads, ""}},
      {"allow-downloads  allow-forms",
       {~WebSandboxFlags::kDownloads & ~WebSandboxFlags::kForms, ""}},

      // Other additional characters aren't ignored:
      {"-allow-downloads",
       {WebSandboxFlags::kAll,
        "'-allow-downloads' is an invalid sandbox flag."}},
      {"+allow-downloads",
       {WebSandboxFlags::kAll,
        "'+allow-downloads' is an invalid sandbox flag."}},

      // Duplicated tokens:
      {"allow-downloads allow-downloads", {~WebSandboxFlags::kDownloads, ""}},

      // When there are multiple errors, the order of the token is preserved,
      // while the duplicates are removed:
      {"b a b a",
       {WebSandboxFlags::kAll, "'b', 'a' are invalid sandbox flags."}},
      {"a b a b",
       {WebSandboxFlags::kAll, "'a', 'b' are invalid sandbox flags."}},

      // Mixing invalid and valid tokens:
      {"allow-downloads invalid",
       {~WebSandboxFlags::kDownloads, "'invalid' is an invalid sandbox flag."}},
      {"invalid allow-downloads",
       {~WebSandboxFlags::kDownloads, "'invalid' is an invalid sandbox flag."}},
      {"a allow-downloads b",
       {~WebSandboxFlags::kDownloads, "'a', 'b' are invalid sandbox flags."}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "input = " << test_case.input);
    WebSandboxFlagsParsingResult output =
        ParseWebSandboxPolicy(test_case.input, WebSandboxFlags::kNone);
    EXPECT_EQ(output.flags, test_case.expected_output.flags);
    EXPECT_EQ(output.error_message, test_case.expected_output.error_message);
  }
}

}  // namespace network
