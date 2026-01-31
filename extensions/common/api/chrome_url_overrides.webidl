// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary UrlOverrideInfo {
  // Override for the chrome://newtab page.
  DOMString newtab;

  // Override for the chrome://bookmarks page.
  DOMString bookmarks;

  // Override for the chrome://history page.
  DOMString history;

  [nodoc] DOMString activationmessage;
  [nodoc] DOMString keyboard;
};

// Stub namespace for the "chrome_url_overrides" manifest key.
[Namespace=chrome_url_overrides]
partial dictionary ExtensionManifest {
  // Chrome url overrides. Note that an extension can override only one page.
  UrlOverrideInfo chrome_url_overrides;
};