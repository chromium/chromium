// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Catches and then logs any errors which occur in `closure` for reporting.
 *
 * @param crweb Flag to identify messages sent by the CrWeb class.
 * @param closure The closure block to be executed.
 * @return The result of running `closure`.
 */
export function catchAndReportErrors(
    crweb: boolean, functionName: string, closure: Function,
    closureArgs?: unknown[]): unknown {
  try {
    return closure.apply(null, closureArgs);
  } catch (error) {
    let errorMessage = '';
    let errorStack = '';
    const is_crweb = crweb;
    if (error && error instanceof Error) {
      errorMessage = error.message;
      if (error.stack) {
        errorStack = error.stack;
      }
      if (errorStack.length > 0) {
        errorStack += '\n';
      }
      errorStack += functionName;
    }
    if (!crweb || (errorMessage && errorStack)) {
      sendWebKitMessage(
        'WindowErrorResultHandler',
        {'message': errorMessage, 'stack': errorStack, 'is_crweb': is_crweb});
    }
  }
  return undefined;
}
