// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_iframe_element_sandbox.h"

#include "base/containers/contains.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

// These are the sandbox tokens which are always supported. If a new token is
// only available behind a runtime flag, it should be checked separately in
// IsTokenSupported below.
const char* const kSupportedSandboxTokens[] = {
    "allow-downloads",
    "allow-forms",
    "allow-modals",
    "allow-orientation-lock",
    "allow-pointer-lock",
    "allow-popups",
    "allow-popups-to-escape-sandbox",
    "allow-presentation",
    "allow-same-origin",
    "allow-scripts",
    "allow-storage-access-by-user-activation",
    "allow-top-navigation",
    "allow-top-navigation-by-user-activation"};

// TODO (https://crbug.com/372894175) move this into |kSupportedSandboxTokens|
// when feature is enabled by default.
constexpr char kAllowSameSiteNoneCookiesSandboxToken[] =
    "allow-same-site-none-cookies";

bool IsTokenSupported(const AtomicString& token) {
  if (base::Contains(kSupportedSandboxTokens, token)) {
    return true;
  }
  return token == kAllowSameSiteNoneCookiesSandboxToken &&
         RuntimeEnabledFeatures::AllowSameSiteNoneCookiesInSandboxEnabled();
}

}  // namespace

HTMLIFrameElementSandbox::HTMLIFrameElementSandbox(
    HTMLFrameOwnerElement* element)
    : DOMTokenList(*element, html_names::kSandboxAttr) {
  DCHECK(IsA<HTMLIFrameElement>(element) ||
         IsA<HTMLFencedFrameElement>(element));
}

bool HTMLIFrameElementSandbox::ValidateTokenValue(
    const AtomicString& token_value,
    ExceptionState&) const {
  return IsTokenSupported(token_value);
}

}  // namespace blink
