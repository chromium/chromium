// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Legacy Navigation related APIs. These APIs are not needed after
 * switching to WKBasedNavigationManager.
 */

goog.provide('__crWeb.legacynavigation');

/** Beginning of anonymouse object */
(function() {

/**
 * Intercept window.history methods to call back/forward natively.
 */
window.history.back = function() {
 __gCrWeb.message.invokeOnHost({'command': 'navigation.goDelta', 'value' : -1});
};

window.history.forward = function() {
 __gCrWeb.message.invokeOnHost({'command': 'navigation.goDelta', 'value' : 1});
};

window.history.go = function(delta) {
  __gCrWeb.message.invokeOnHost(
      {'command': 'navigation.goDelta', 'value': delta | 0});
};

/** Flush the message queue. */
if (__gCrWeb.message) {
  __gCrWeb.message.invokeQueues();
}

}());  // End of anonymouse object
