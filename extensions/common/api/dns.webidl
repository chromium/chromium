// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary ResolveCallbackResolveInfo {
  // The result code. Zero indicates success.
  required long resultCode;

  // A string representing the IP address literal. Supplied only if resultCode
  // indicates success.
  DOMString address;
};

// Use the <code>chrome.dns</code> API for dns resolution.
interface Dns {
  // Resolves the given hostname or IP address literal.
  // |hostname| : The hostname to resolve.
  // |Returns|: Called when the resolution operation completes.
  // |PromiseValue|: resolveInfo
  [requiredCallback] static Promise<ResolveCallbackResolveInfo> resolve(
      DOMString hostname);
};

partial interface Browser {
  static attribute Dns dns;
};
