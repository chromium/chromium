// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Overrides the navigator.clipboard API methods to intercept
 * them and send them to the native code for approval.
 *
 * This script ensures that calls to the clipboard API are handled sequentially,
 * as specified by https://w3c.github.io/clipboard-apis/#clipboard-task-source.
 * If method call A happens before method call B, it is guaranteed that the
 * promise for method call A will be settled (fulfilled or rejected) before the
 * promise for method call B is. If the browser sends a response for method
 * call B before it has responded to method call A, then the promise for A will
 * be rejected, and only then will the promise for B be settled.
 */

import {sendDidFinishClipboardReadMessage} from '//ios/web/js_features/clipboard/resources/clipboard_util.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * The maximum number of clipboard requests that can be queued before the
 * oldest one is automatically rejected.
 */
const MAX_PENDING_REQUESTS = 100;

/**
 * Represents a call to a method in the clipboard API that is waiting for
 * settlement (fulfillment or rejection) from the browser.
 */
interface PendingRequest {
  /**
   * The unique ID for this request.
   */
  requestId: number;
  /**
   * The function to call to fulfill the promise. This will call the original
   * native clipboard function.
   */
  fulfill: () => void;
  /**
   * The function to call to reject the promise. This is used when the browser
   * denies the request or when the request is superseded by a newer one.
   */
  reject: (reason?: Error) => void;
}

// A list of all pending clipboard requests, in the order they were made.
const pendingClipboardRequests: PendingRequest[] = [];

// The ID to be assigned to the next clipboard request.
let nextRequestId = 0;

// The original native functions, saved before they are overridden.
let originalWriteText: ((text: string) => Promise<void>)|null = null;
let originalWrite: ((data: ClipboardItem[]) => Promise<void>)|null = null;
let originalReadText: (() => Promise<string>)|null = null;
let originalRead: (() => Promise<ClipboardItems>)|null = null;

/**
 * Creates a function that overrides a navigator.clipboard method.
 * The returned function intercepts the call, sends a message to the browser
 * for approval, and only calls the original function after approval is granted.
 * @template T The return type of the original function's promise.
 * @template A The arguments array type of the original function.
 * @param originalFunction The original clipboard method to be called after
 *     approval.
 * @param command The command to send to the native message handler ('write' or
 *     'read').
 * @returns A new function that can be used to override the original method.
 */
function createClipboardOverride<T, A extends any[]>(
    originalFunction: (this: Clipboard, ...args: A) => Promise<T>,
    command: string): (...args: A) => Promise<T> {
  // Create a new function that will replace the original clipboard method.
  return function(this: Clipboard, ...args: A): Promise<T> {
    // If there are too many pending requests, evict the oldest one.
    if (pendingClipboardRequests.length >= MAX_PENDING_REQUESTS) {
      const oldestRequest = pendingClipboardRequests.shift();
      if (oldestRequest) {
        oldestRequest.reject(new Error('Too many pending clipboard requests.'));
      }
    }

    const requestId = nextRequestId++;
    // Create a new promise that will be returned to the caller. This promise
    // will not be settled until the browser responds with approval.
    const promise = new Promise<T>((resolve, reject) => {
      pendingClipboardRequests.push({
        requestId: requestId,
        // When the browser approves, the original function will be called.
        // The new promise will be settled with the result of the original
        // function's promise.
        fulfill: () => {
          originalFunction.apply(this, args)
              .then(resolve, reject)
              .finally(() => {
                if (command === 'read') {
                  sendDidFinishClipboardReadMessage();
                }
              });
        },
        reject: reject,
      });
    });

    sendWebKitMessage('ClipboardHandler', {
      'frameId': gCrWeb.getFrameId(),
      'command': command,
      'requestId': requestId,
    });

    return promise;
  };
}

/**
 * Installs overrides for the navigator.clipboard API. After the first call,
 * subsequent calls are a no-op. This is a no-op if the API is not available
 * (e.g., in an insecure context).
 */
function installClipboardOverrides() {
  // TODO(crbug.com/439544714): Override document.execCommand to intercept
  // 'copy', 'paste', and 'cut' commands for a more comprehensive clipboard
  // interception.
  // The navigator.clipboard API is only available in secure contexts (HTTPS).
  if (!navigator.clipboard || originalWriteText) {
    return;
  }

  originalWriteText = navigator.clipboard.writeText;
  originalWrite = navigator.clipboard.write;
  originalReadText = navigator.clipboard.readText;
  originalRead = navigator.clipboard.read;

  navigator.clipboard.writeText =
      createClipboardOverride(originalWriteText, 'write');
  navigator.clipboard.write = createClipboardOverride(originalWrite, 'write');
  navigator.clipboard.readText =
      createClipboardOverride(originalReadText, 'read');
  navigator.clipboard.read = createClipboardOverride(originalRead, 'read');
}

/**
 * Processes the browser's response to a clipboard request.
 * @param resolvedRequestId The ID of the request that has been processed.
 * @param isAllowed Whether the browser approved the request.
 */
function resolveRequest(resolvedRequestId: number, isAllowed: boolean): void {
  let requestResolved = false;

  // Reject all pending requests before `resolvedRequestId` and settle the
  // target request.
  while (pendingClipboardRequests.length > 0) {
    const pendingRequest = pendingClipboardRequests.shift();
    if (pendingRequest) {
      if (pendingRequest.requestId === resolvedRequestId) {
        requestResolved = true;
        // Settle the clipboard request.
        if (isAllowed) {
          pendingRequest.fulfill();
        } else {
          pendingRequest.reject(new Error('Clipboard access denied.'));
        }
        // Newer requests should be left pending.
        break;
      } else {
        // Reject any pending requests happening before `resolvedRequestId`.
        pendingRequest.reject(
            new Error('Clipboard request was superseded by a newer one.'));
      }
    }
  }

  if (!requestResolved) {
    throw new Error(
        'Attempted to resolve clipboard request that was not pending. ' +
        'Request id: ' + resolvedRequestId);
  }
}

const clipboardApi = new CrWebApi();
clipboardApi.addFunction('resolveRequest', resolveRequest);
gCrWeb.registerApi('clipboard', clipboardApi);

installClipboardOverrides();
