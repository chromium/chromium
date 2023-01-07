// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_iframe_element_sandbox.h"

#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

// These are the sandbox tokens which are always supported. If a new token is
// only available behind a runtime flag, it should be checked separately in
// IsTokenSupported below.
const char* const kSupportedSandboxTokens[] = {
    "allow-forms",
    "allow-modals",
    "allow-orientation-lock",
    "allow-pointer-lock",
    "allow-popups",
    "allow-popups-to-escape-sandbox",
    "allow-presentation",
    "allow-same-origin",
    "allow-scripts",
    "allow-top-navigation",
    "allow-top-navigation-by-user-activation",
    "allow-downloads"};

// TODO (http://crbug.com/989663) move this into |kSupportedSandboxTokens| when
// feature flag is enabled by default.
constexpr char kStorageAccessAPISandboxToken[] =
    "allow-storage-access-by-user-activation";

bool IsTokenSupported(const AtomicString& token) {
  for (const char* supported_token : kSupportedSandboxTokens) {
    if (token == supported_token)
      return true;
  }

  // The Storage Access API and corresponding sandbox token is behind the
  // |StorageAccessAPI| runtimeflag. Only check this token if
  // the feature is enabled.
  if (RuntimeEnabledFeatures::StorageAccessAPIEnabled() &&
      (token == kStorageAccessAPISandboxToken)) {
    return true;
  }

  return false;
}

}  // namespace

HTMLIFrameElementSandbox::HTMLIFrameElementSandbox(HTMLIFrameElement* element)
    : DOMTokenList(*element, html_names::kSandboxAttr) {}

bool HTMLIFrameElementSandbox::ValidateTokenValue(
    const AtomicString& token_value,
    ExceptionState&) const {
  return IsTokenSupported(token_value);
}

}  // namespace blink
