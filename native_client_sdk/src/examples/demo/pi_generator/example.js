// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handle a message coming from the NaCl module.  The message payload is
// assumed to contain the current estimated value of Pi.  Update the Pi
// text display with this value.
function handleMessage(message_event) {
  document.getElementById('pi').value = message_event.data;
}
