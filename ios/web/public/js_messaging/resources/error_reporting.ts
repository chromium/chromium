// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {clearAllCrashKeys, getCrashKeys} from '//ios/web/js_features/crash_keys/resources/crash_keys.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Catches and then logs any errors which occur in `closure` for reporting.
 *
 * @param closure The closure block to be executed.
 * @return The result of running `closure`.
 */
export function catchAndReportErrors(
    apiName: string, closure: Function, closureArgs?: unknown[]): unknown {
  try {
    return closure.apply(null, closureArgs);
  } catch (error) {
    let errorMessage = '';
    let errorStack = '';
    if (error && error instanceof Error) {
      errorMessage = error.message;
      if (error.stack) {
        errorStack = error.stack;
      }
    }
    if (errorMessage && errorStack) {
      const crashKeys = getCrashKeys();
      clearAllCrashKeys();
      sendWebKitMessage('WindowErrorResultHandler', {
        'message': errorMessage,
        'stack': errorStack,
        'api': apiName,
        'crashKeys': crashKeys,
      });
    }
  }
  return undefined;
}
