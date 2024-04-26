// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Navigation related APIs.
 */

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

declare interface Message {
  command: 'willChangeState' | 'didPushState' | 'didReplaceState',
  frame_id: string,
  stateObject?: string,
  baseUrl?: string,
  pageUrl?: string,
}

class DataCloneError {
  // The name and code for this error are defined by the WebIDL spec. See
  // https://heycam.github.io/webidl/#datacloneerror
  name = 'DataCloneError';
  code = 25;
  message = 'Cyclic structures are not supported.';
}

// Stores queued messages until they can be sent to the "NavigationEventMessage"
// handler.
class MessageQueue {
    queuedMessages: Message[] = [];

    // Attempts to send any queued messages. Messages
    // will be only be removed once they have been sent.
    sendQueuedMessages(): void {
      while (this.queuedMessages.length > 0) {
        try {
          sendWebKitMessage(
              'NavigationEventMessage', this.queuedMessages[0] as Message);
          this.queuedMessages.shift()
        } catch (e) {
          // 'NavigationEventMessage' message handler
          // is not currently registered. Send the
          // message later when possible.
          break;
        }
      }
    };

    // Queues the `message` and triggers the queue to be sent.
    queueNavigationEventMessage(message: Message): void {
      this.queuedMessages.push(message);
      this.sendQueuedMessages();
    };
}

const messageQueue = new MessageQueue();

/**
 * Retain the original JSON.stringify method where possible to reduce the
 * impact of sites overriding it
 */
const JSONStringify: Function = JSON.stringify;

/**
 * Keep the original pushState() and replaceState() methods. It's needed to
 * update the web view's URL and window.history.state property during history
 * navigations that don't cause a page load.
 */
const originalWindowHistoryPushState = window.history.pushState;
const originalWindowHistoryReplaceState = window.history.replaceState;

/**
 * Intercepts window.history methods so native code can differentiate between
 * same-document navigation that are state navigations vs. hash navigations.
 * This is needed for backward compatibility of DidStartLoading, which is
 * triggered for fragment navigation but not state navigation.
 * TODO(crbug.com/41354482): Remove this once DidStartLoading is no longer
 * called for same-document navigation.
 */
History.prototype.pushState =
    function(stateObject: object, pageTitle: string,
        pageUrl: string | URL): void {
  messageQueue.queueNavigationEventMessage({
    'command': 'willChangeState',
    'frame_id': gCrWeb.message.getFrameId()
  });

  // JSONStringify throws an exception when given a cyclical object. This
  // internal implementation detail should not be exposed to callers of
  // pushState. Instead, throw a standard exception when stringification fails.
  let serializedState = '';
  try {
    // Calling stringify() on undefined causes a JSON parse error.
    if (typeof (stateObject) != 'undefined') {
      serializedState = JSONStringify(stateObject);
    }
  } catch (e) {
    throw new DataCloneError();
  }
  pageUrl = pageUrl || window.location.href;
  originalWindowHistoryPushState.call(history, stateObject, pageTitle, pageUrl);
  messageQueue.queueNavigationEventMessage({
    'command': 'didPushState',
    'stateObject': serializedState,
    'baseUrl': document.baseURI,
    'pageUrl': pageUrl.toString(),
    'frame_id': gCrWeb.message.getFrameId()
  });
};

History.prototype.replaceState =
    function(stateObject: object, pageTitle: string,
        pageUrl: string | URL): void {
  messageQueue.queueNavigationEventMessage({
    'command': 'willChangeState',
    'frame_id': gCrWeb.message.getFrameId()
  });

  // JSONStringify throws an exception when given a cyclical object. This
  // internal implementation detail should not be exposed to callers of
  // replaceState. Instead, throw a standard exception when stringification
  // fails.
  let serializedState = '';
  try {
    // Calling stringify() on undefined causes a JSON parse error.
    if (typeof (stateObject) != 'undefined') {
      serializedState = JSONStringify(stateObject);
    }
  } catch (e) {
    throw new DataCloneError();
  }
  pageUrl = pageUrl || window.location.href;
  originalWindowHistoryReplaceState.call(
      history, stateObject, pageTitle, pageUrl);
  messageQueue.queueNavigationEventMessage({
    'command': 'didReplaceState',
    'stateObject': serializedState,
    'baseUrl': document.baseURI,
    'pageUrl': pageUrl.toString(),
    'frame_id': gCrWeb.message.getFrameId()
  });
};
