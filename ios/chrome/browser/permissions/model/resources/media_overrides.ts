// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js'

// Ensure that the API is not added if it does not already exist.
if (typeof navigator === 'object' && 'mediaDevices' in navigator &&
    'getUserMedia' in navigator.mediaDevices) {
  const originalFunc = navigator.mediaDevices.getUserMedia;
  navigator.mediaDevices.getUserMedia = function() {
    const details: {[key: string]: boolean} = {};
    // Use a try block to ensure that attempting to parse parameters does not
    // break the original API functionality.
    try {
      if (arguments.length > 0) {
        const constraints = arguments[0];
        // `constraints` may contain objects, so convert to a boolean in
        // `details` to ensure it can be sent using sendWebKitMessage.
        details['audio'] = constraints['audio'] == true;
        details['video'] = constraints['video'] == true;
      }
    } catch (error) {
      // Argument parsing error can be ignored here, empty details state will be
      // logged on native side.
    }
    sendWebKitMessage('MediaAPIAccessedHandler', details);
    const originalArgs = arguments as
        unknown as [constraints?: MediaStreamConstraints|undefined];
    return originalFunc.apply(this, originalArgs)
  }
}
