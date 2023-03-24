// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The ATP V8 instance does not define 'self', which is needed by mojom.
let self = this;

// ATP doesn't have console.* yet, so shim these over.
let console = atpconsole;
console.assert = (check, message) => {
  if (!check) {
    atpconsole.error(message);
  }
};
