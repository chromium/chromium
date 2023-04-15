// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Navigation related APIs.
 */

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
  this.message = 'Cyclic structures are not supported.';
}

// Stores queued messages until they can be sent to the "NavigationEventMessage"
// handler.
var queuedMessages = [];

// Attempts to send any queued messages. Messages will be only be removed once
// they have been sent.
function sendQueuedMessages() {
  while (queuedMessages.length > 0) {
    try {
      __gCrWeb.common.sendWebKitMessage(
          'NavigationEventMessage', queuedMessages[0]);
      queuedMessages.shift();
    } catch (e) {
      // 'NavigationEventMessage' message handler is not currently registered.
      // Send the message later when possible.
      break;
    }
  }
};

// Queues the |message| and triggers the queue to be sent.
function queueNavigationEventMessage(message) {
  queuedMessages.push(message);
  sendQueuedMessages();
};

/**
 * Intercepts window.history methods so native code can differentiate between
 * same-document navigation that are state navigations vs. hash navigations.
 * This is needed for backward compatibility of DidStartLoading, which is
 * triggered for fragment navigation but not state navigation.
 * TODO(crbug.com/783382): Remove this once DidStartLoading is no longer
 * called for same-document navigation.
 */
History.prototype.pushState = function(stateObject, pageTitle, pageUrl) {
  queueNavigationEventMessage({
    'command': 'willChangeState',
    'frame_id': __gCrWeb.message.getFrameId()
  });

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
  queueNavigationEventMessage({
    'command': 'didPushState',
    'stateObject': serializedState,
    'baseUrl': document.baseURI,
    'pageUrl': pageUrl.toString(),
    'frame_id': __gCrWeb.message.getFrameId()
  });
};

History.prototype.replaceState = function(stateObject, pageTitle, pageUrl) {
  queueNavigationEventMessage({
    'command': 'willChangeState',
    'frame_id': __gCrWeb.message.getFrameId()
  });

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
  queueNavigationEventMessage({
    'command': 'didReplaceState',
    'stateObject': serializedState,
    'baseUrl': document.baseURI,
    'pageUrl': pageUrl.toString(),
    'frame_id': __gCrWeb.message.getFrameId()
  });
};
