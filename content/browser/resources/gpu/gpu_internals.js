// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="browser_bridge.js">
// <include src="info_view.js">
// <include src="vulkan_info.js">

let browserBridge;

/**
 * Main entry point. called once the page has loaded.
 */
function onLoad() {
  browserBridge = new gpu.BrowserBridge();

  // Create the views.
  cr.ui.decorate('#info-view', gpu.InfoView);
}

document.addEventListener('DOMContentLoaded', onLoad);
