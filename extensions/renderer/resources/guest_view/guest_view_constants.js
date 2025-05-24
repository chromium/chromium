// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module contains constants used in guestview.

// Container for the guestview constants.
var GuestViewConstants = {
  // Error/warning messages.
  ERROR_MSG_CALLBACK_NOT_ALLOWED:
      'Callback form deprecated, see API doc for correct usage.',
};

function tagLogMessage(tag, message) {
  return `<${tag}>: ${message}`;
}

exports.$set('GuestViewConstants', $Object.freeze(GuestViewConstants));
exports.$set('tagLogMessage', tagLogMessage);
