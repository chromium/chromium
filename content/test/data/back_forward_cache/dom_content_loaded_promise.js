// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Set up a promise that will be resolve when DOMContentLoaded is fired.
var domContentLoaded = new Promise(resolve => {
  document.addEventListener("DOMContentLoaded", () => resolve(42));
});
