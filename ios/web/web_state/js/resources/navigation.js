// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Navigation related APIs.
 */

goog.provide('__crWeb.navigation');

/** Beginning of anonymouse object */
(function() {

/**
 * Keep the original pushState() and replaceState() methods. It's needed to
 * update the web view's URL and window.history.state property during history
 * navigations that don't cause a page load.
 * @private
 */
var originalWindowHistoryPushState = window.history.pushState;
var originalWindowHistoryReplaceState = window.history.replaceState;

function DataCloneError() {
  // The name and code for this error are defined by the WebIDL spec. See
  // https://heycam.github.io/webidl/#datacloneerror
  this.name = 'DataCloneError';
  this.code = 25;
  this.message = "Cyclic structures are not supported.";
}

/**
 * Intercepts window.history methods so native code can differentiate between
 * same-document navigation that are state navigations vs. hash navigations.
 * This is needed for backward compatibility of DidStartLoading, which is
 * triggered for fragment navigation but not state navigation.
 * TODO(crbug.com/783382): Remove this once DidStartLoading is no longer
 * called for same-document navigation.
 */
window.history.pushState = function(stateObject, pageTitle, pageUrl) {
  __gCrWeb.message.invokeOnHost({'command': 'navigation.willChangeState'});

  // JSONStringify throws an exception when given a cyclical object. This
  // internal implementation detail should not be exposed to callers of
  // pushState. Instead, throw a standard exception when stringification fails.
  try {
    // Calling stringify() on undefined causes a JSON parse error.
    var serializedState = typeof (stateObject) == 'undefined' ?
        '' :
        __gCrWeb.common.JSONStringify(stateObject);
  } catch (e) {
    throw new DataCloneError();
  }
  pageUrl = pageUrl || window.location.href;
  originalWindowHistoryPushState.call(history, stateObject, pageTitle, pageUrl);
  __gCrWeb.message.invokeOnHost({
    'command': 'navigation.didPushState',
    'stateObject': serializedState,
    'baseUrl': document.baseURI,
    'pageUrl': pageUrl.toString()
  });
};

window.history.replaceState = function(stateObject, pageTitle, pageUrl) {
  __gCrWeb.message.invokeOnHost({'command': 'navigation.willChangeState'});

 // JSONStringify throws an exception when given a cyclical object. This
 // internal implementation detail should not be exposed to callers of
 // replaceState. Instead, throw a standard exception when stringification
 // fails.
  try {
    // Calling stringify() on undefined causes a JSON parse error.
    var serializedState = typeof (stateObject) == 'undefined' ?
        '' :
        __gCrWeb.common.JSONStringify(stateObject);
  } catch (e) {
    throw new DataCloneError();
  }
  pageUrl = pageUrl || window.location.href;
  originalWindowHistoryReplaceState.call(
      history, stateObject, pageTitle, pageUrl);
  __gCrWeb.message.invokeOnHost({
    'command': 'navigation.didReplaceState',
    'stateObject': serializedState,
    'baseUrl': document.baseURI,
    'pageUrl': pageUrl.toString()
  });
};

window.addEventListener('hashchange', function(evt) {
  __gCrWeb.message.invokeOnHost({'command': 'navigation.hashchange'});
});

/** Flush the message queue. */
if (__gCrWeb.message) {
  __gCrWeb.message.invokeQueues();
}
}());  // End of anonymouse object
