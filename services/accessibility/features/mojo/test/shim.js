// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// ATP doesn't have console.* yet, so shim these over.
const console = atpconsole;
console.assert = (check, message) => {
  if (!check) {
    atpconsole.error(message);
  }
};
