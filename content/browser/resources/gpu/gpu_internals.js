// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_view.js';

import {BrowserBridge} from './browser_bridge.js';

// Injected script from C++ or test environments may reference `browserBridge`
// as a property of the global object.
window.browserBridge = new BrowserBridge();

/**
 * Main entry point. called once the page has loaded.
 */
function onLoad() {
  // Create the views.
  document.querySelector('info-view')
      .addBrowserBridgeListeners(window.browserBridge);

  // Because of inherent raciness (between the deprecated DevTools API which
  // telemtry uses to drive the relevant tests, and the asynchronous loading of
  // JS modules like this one) it's possible for telemetry tests to inject code
  // *before* `browserBridge` is set and the DOM is populated. This flag is used
  // to synchronize script injection by tests to prevent such races.
  window.gpuPagePopulated = true;
}

document.addEventListener('DOMContentLoaded', onLoad);
