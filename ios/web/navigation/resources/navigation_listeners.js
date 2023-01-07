// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Navigation listener to report hash change.
 */

window.addEventListener('hashchange', function(evt) {
  __gCrWeb.common.sendWebKitMessage(
      'NavigationEventMessage',
      {'command': 'hashchange', 'frame_id': __gCrWeb.message.getFrameId()});
});
