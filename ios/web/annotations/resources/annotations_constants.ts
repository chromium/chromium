// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tags that should not be visited.
const NON_TEXT_NODE_NAMES = new Set([
  'SCRIPT', 'NOSCRIPT', 'STYLE', 'EMBED', 'OBJECT', 'TEXTAREA', 'IFRAME',
  'INPUT'
]);

export {
  NON_TEXT_NODE_NAMES,
}