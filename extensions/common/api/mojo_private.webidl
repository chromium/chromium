// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The chrome.mojoPrivate API provides access to the mojo modules.
interface MojoPrivate {
  // Returns a promise that will resolve to an asynchronously
  // loaded module.
  [nocompile] static any requireAsync(DOMString name);
};

partial interface Browser {
  static attribute MojoPrivate mojoPrivate;
};
