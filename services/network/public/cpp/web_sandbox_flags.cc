// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/web_sandbox_flags.h"
#include <set>
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"

namespace network {

using mojom::WebSandboxFlags;
namespace {

// See: https://infra.spec.whatwg.org/#ascii-whitespace
// This is different from: base::kWhitespaceASCII.
const char* kHtmlWhitespace = " \n\t\r\f";

WebSandboxFlags ParseWebSandboxToken(const base::StringPiece& token) {
  constexpr struct {
    const char* token;
    WebSandboxFlags flags;
  } table[] = {
      {"allow-downloads", WebSandboxFlags::kDownloads},
      {"allow-forms", WebSandboxFlags::kForms},
      {"allow-modals", WebSandboxFlags::kModals},
      {"allow-orientation-lock", WebSandboxFlags::kOrientationLock},
      {"allow-pointer-lock", WebSandboxFlags::kPointerLock},
      {"allow-popups", WebSandboxFlags::kPopups},
      {"allow-popups-to-escape-sandbox",
       WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts},
      {"allow-presentation", WebSandboxFlags::kPresentationController},
      {"allow-same-origin", WebSandboxFlags::kOrigin},
      {"allow-scripts",
       WebSandboxFlags::kAutomaticFeatures | WebSandboxFlags::kScripts},
      {"allow-storage-access-by-user-activation",
       WebSandboxFlags::kStorageAccessByUserActivation},
      {"allow-top-navigation", WebSandboxFlags::kTopNavigation},
      {"allow-top-navigation-by-user-activation",
       WebSandboxFlags::kTopNavigationByUserActivation},
  };

  for (const auto& it : table) {
    if (CompareCaseInsensitiveASCII(it.token, token) == 0)
      return it.flags;
  }

  return WebSandboxFlags::kNone;  // Not found.
}

}  // namespace

// See: http://www.w3.org/TR/html5/the-iframe-element.html#attr-iframe-sandbox
WebSandboxFlagsParsingResult ParseWebSandboxPolicy(
    const base::StringPiece& input,
    WebSandboxFlags ignored_flags) {
  WebSandboxFlagsParsingResult out;
  out.flags = WebSandboxFlags::kAll;

  std::vector<base::StringPiece> error_tokens;
  for (const auto& token :
       base::SplitStringPiece(input, kHtmlWhitespace, base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    WebSandboxFlags flags = ~ParseWebSandboxToken(token);
    flags |= ignored_flags;
    out.flags &= flags;
    if (flags == WebSandboxFlags::kAll)
      error_tokens.push_back(token);
  }

  if (!error_tokens.empty()) {
    // Some tests expect the order of error tokens to be preserved, while
    // removing the duplicates:
    // See /fast/frames/sandboxed-iframe-attribute-parsing-03.html
    std::set<base::StringPiece> set;
    base::EraseIf(error_tokens, [&](auto x) { return !set.insert(x).second; });

    out.error_message =
        "'" + base::JoinString(error_tokens, "', '") +
        (error_tokens.size() > 1 ? "' are invalid sandbox flags."
                                 : "' is an invalid sandbox flag.");
  }

  return out;
}

}  // namespace network
