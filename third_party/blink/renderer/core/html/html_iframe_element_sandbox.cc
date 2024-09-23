// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_iframe_element_sandbox.h"

#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"

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

  return token == kStorageAccessAPISandboxToken;
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
