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
 * A popstate event needs to be fired anytime the active history entry
 * changes without an associated document change. Either via back, forward, go
 * navigation or by loading the URL, clicking on a link, etc.
 */
__gCrWeb['dispatchPopstateEvent'] = function(stateObject) {
  var popstateEvent = window.document.createEvent('HTMLEvents');
  popstateEvent.initEvent('popstate', true, false);
  if (stateObject)
    popstateEvent.state = JSON.parse(stateObject);

  // setTimeout() is used in order to return immediately. Otherwise the
  // dispatchEvent call waits for all event handlers to return, which could
  // cause a ReentryGuard failure.
  window.setTimeout(function() {
    window.dispatchEvent(popstateEvent);
  }, 0);
};

/**
 * A hashchange event needs to be fired after a same-document history
 * navigation between two URLs that are equivalent except for their fragments.
 */
__gCrWeb['dispatchHashchangeEvent'] = function(oldURL, newURL) {
  var hashchangeEvent = window.document.createEvent('HTMLEvents');
  hashchangeEvent.initEvent('hashchange', true, false);
  if (oldURL)
    hashchangeEvent.oldURL = oldURL;
  if (newURL)
    hashchangeEvent.newURL = newURL;

  // setTimeout() is used in order to return immediately. Otherwise the
  // dispatchEvent call waits for all event handlers to return, which could
  // cause a ReentryGuard failure.
  window.setTimeout(function() {
    window.dispatchEvent(hashchangeEvent);
  }, 0);
};

/**
 * Keep the original pushState() and replaceState() methods. It's needed to
 * update the web view's URL and window.history.state property during history
 * navigations that don't cause a page load.
 * @private
 */
var originalWindowHistoryPushState = window.history.pushState;
var originalWindowHistoryReplaceState = window.history.replaceState;

__gCrWeb['replaceWebViewURL'] = function(url, stateObject) {
  originalWindowHistoryReplaceState.call(history, stateObject, '', url);
};

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
  // Because hash changes don't trigger __gCrWeb.didFinishNavigation, so fetch
  // favicons for the new page manually.
  __gCrWeb.message.invokeOnHost({
    'command': 'favicon.favicons',
    'favicons': __gCrWeb.common.getFavicons()
  });

  __gCrWeb.message.invokeOnHost({'command': 'navigation.hashchange'});
});

/** Flush the message queue. */
if (__gCrWeb.message) {
  __gCrWeb.message.invokeQueues();
}
}());  // End of anonymouse object
