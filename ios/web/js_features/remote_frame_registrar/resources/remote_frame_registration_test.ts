// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test access into the Child Frame Registration lib.
 * Requires functions in child_frame_registration_lib.ts.
 */

import {processChildFrameMessage, registerChildFrame} from '//ios/web/js_features/remote_frame_registrar/resources/remote_frame_registration.js';
/**
 * Calls registerChildFrame on each frame in the document. This is a convenience
 * method for testing from the C++ layer.
 * @return {string[]} The list of remote IDs sent to the child frames.
 */
export function registerAllChildFrames(): string[] {
  const ids: string[] = [];
  for (const frame of document.getElementsByTagName('iframe')) {
    ids.push(registerChildFrame((frame as HTMLIFrameElement)));
  }
  return ids;
}

window.addEventListener('message', processChildFrameMessage);
