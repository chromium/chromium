// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Catches and then logs any errors which occur in `closure` for reporting.
 *
 * @param closure The closure block to be executed.
 * @return The result of running `closure`.
 */
function catchAndReportErrors(
    closure: Function, closureArgs?: unknown[]): unknown {
  try {
    return closure.apply(null, closureArgs);
  } catch (error) {
    var errorMessage = '';
    var errorStack = '';
    if (error && error instanceof Error) {
      errorMessage = error.message;
      if (error.stack) {
        errorStack = error.stack;
      }
    }
    sendWebKitMessage(
        'WindowErrorResultHandler',
        {'message': errorMessage, 'stack': errorStack});
  }
  return undefined;
};

export {catchAndReportErrors}
