// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Requires functions from favicon.js

window.addEventListener('hashchange', function(evt) {
  // Hash changes don't trigger __gCrWeb.didFinishNavigation, so fetch favicons
  // for the new page manually.
  __gCrWeb.favicon.sendFaviconUrls();
});
