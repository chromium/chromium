// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { sendFaviconUrls } from "//ios/web/favicon/resources/favicon_utils.js";

window.addEventListener('hashchange', () => {
  // Manually update favicons because hash changes do not trigger
  // a reload of injected scripts.
  // (Script injection is the only time when favicon.ts send favicons.)
  sendFaviconUrls();
});