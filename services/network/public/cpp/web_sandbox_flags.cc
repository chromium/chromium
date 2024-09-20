// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/web_sandbox_flags.h"

#include <set>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"

namespace network {

using mojom::WebSandboxFlags;
namespace {

// See: https://infra.spec.whatwg.org/#ascii-whitespace
// This is different from: base::kWhitespaceASCII.
const char* kHtmlWhitespace = " \n\t\r\f";

WebSandboxFlags ParseWebSandboxToken(std::string_view token) {
  using enum WebSandboxFlags;
  static constexpr auto kTokenToWebSandboxFlags =
      base::MakeFixedFlatMap<std::string_view, WebSandboxFlags>({
          {"allow-downloads", kDownloads},
          {"allow-forms", kForms},
          {"allow-modals", kModals},
          {"allow-orientation-lock", kOrientationLock},
          {"allow-pointer-lock", kPointerLock},
          {"allow-popups", kPopups | kTopNavigationToCustomProtocols},
          {"allow-popups-to-escape-sandbox",
           kPropagatesToAuxiliaryBrowsingContexts},
          {"allow-presentation", kPresentationController},
          {"allow-same-origin", kOrigin},
          {"allow-scripts", kAutomaticFeatures | kScripts},
          {"allow-storage-access-by-user-activation",
           kStorageAccessByUserActivation},
          {"allow-top-navigation",
           kTopNavigation | kTopNavigationToCustomProtocols},
          {"allow-top-navigation-by-user-activation",
           kTopNavigationByUserActivation},
          {"allow-top-navigation-to-custom-protocols",
           kTopNavigationToCustomProtocols},
      });

  std::string lowered_token = base::ToLowerASCII(token);
  const auto it = kTokenToWebSandboxFlags.find(lowered_token);
  return it == kTokenToWebSandboxFlags.end() ? kNone : it->second;
}

}  // namespace

// See: http://www.w3.org/TR/html5/the-iframe-element.html#attr-iframe-sandbox
WebSandboxFlagsParsingResult ParseWebSandboxPolicy(
    std::string_view input,
    WebSandboxFlags ignored_flags) {
  WebSandboxFlagsParsingResult out;
  out.flags = WebSandboxFlags::kAll;

  std::vector<std::string_view> error_tokens;
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
    std::set<std::string_view> set;
    std::erase_if(error_tokens, [&](auto x) { return !set.insert(x).second; });

    out.error_message =
        "'" + base::JoinString(error_tokens, "', '") +
        (error_tokens.size() > 1 ? "' are invalid sandbox flags."
                                 : "' is an invalid sandbox flag.");
  }

  return out;
}

}  // namespace network
