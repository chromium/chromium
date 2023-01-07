// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module contains the public-facing API functions for the <webview> tag.

var WEB_VIEW_API_METHODS = [
  // Add content scripts for the guest page.
  'addContentScripts',

  // Navigates to the previous history entry.
  'back',

  // Returns whether there is a previous history entry to navigate to.
  'canGoBack',

  // Returns whether there is a subsequent history entry to navigate to.
  'canGoForward',

  // Captures the visible region of the WebView contents into a bitmap.
  'captureVisibleRegion',

  // Clears browsing data for the WebView partition.
  'clearData',

  // Injects JavaScript code into the guest page.
  'executeScript',

  // Initiates a find-in-page request.
  'find',

  // Navigates to the subsequent history entry.
  'forward',

  // Returns audio state.
  'getAudioState',

  // Returns Chrome's internal process ID for the guest web page's current
  // process.
  'getProcessId',

  // Returns the user agent string used by the webview for guest page requests.
  'getUserAgent',

  // Gets the current zoom factor.
  'getZoom',

  // Gets the current zoom mode of the webview.
  'getZoomMode',

  // Navigates to a history entry using a history index relative to the current
  // navigation.
  'go',

  // Injects CSS into the guest page.
  'insertCSS',

  // Returns whether audio is muted.
  'isAudioMuted',

  // Returns whether spatial navigation is enabled.
  'isSpatialNavigationEnabled',

  // Indicates whether or not the webview's user agent string has been
  // overridden.
  'isUserAgentOverridden',

  // Loads a data URL with a specified base URL used for relative links.
  // Optionally, a virtual URL can be provided to be shown to the user instead
  // of the data URL.
  'loadDataWithBaseUrl',

  // Prints the contents of the webview.
  'print',

  // Removes content scripts for the guest page.
  'removeContentScripts',

  // Reloads the current top-level page.
  'reload',

  // Set audio mute.
  'setAudioMuted',

  // Set spatial navigation state.
  'setSpatialNavigationEnabled',

  // Override the user agent string used by the webview for guest page requests.
  'setUserAgentOverride',

  // Changes the zoom factor of the page.
  'setZoom',

  // Changes the zoom mode of the webview.
  'setZoomMode',

  // Stops loading the current navigation if one is in progress.
  'stop',

  // Ends the current find session.
  'stopFinding',

  // Forcibly kills the guest web page's renderer process.
  'terminate'
];

// Exports.
exports.$set('WEB_VIEW_API_METHODS', WEB_VIEW_API_METHODS);
