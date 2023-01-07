// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mirrored in src/chrome/test/remoting/remote_test_helper.h
var Action = {
  Error: 0,
  None: 1,
  Keydown: 2,
  Buttonpress: 3,
  Mousemove: 4,
  Mousewheel: 5,
  Drag: 6
};

/**
 * Method to handle sending keypress events to the server.
 * @param {Event} event The event handler event.
 */
function handleKeyPress(event) {
  jsonRpc.setLastEvent(Action.Keydown, event.keyCode, 0);
}

/**
 * Method to handle sending mouse down events to the server.
 * @param {Event} event The event handler event.
 */
function handleMouseDown(event) {
  jsonRpc.setLastEvent(Action.Buttonpress, event.button, 0);
}

/**
 * Method to handle sending mouse move events to the server.
 * @param {Event} event The event handler event.
 */
function handleMouseMove(event) {
  jsonRpc.setLastEvent(Action.Mousemove, event.keyCode, 0);
}

/**
 * Method to handle sending mouse wheel events to the server.
 * @param {Event} event The event handler event.
 */
function handleMouseWheel(event) {
  jsonRpc.setLastEvent(Action.Mousewheel, 0, 0);
}

/**
 * Method to handle sending drag events to the server.
 * @param {Event} event The event handler event.
 */
function handleDrag(event) {
  jsonRpc.setLastEvent(Action.Drag, 0, 0);
}

window.addEventListener('keydown', handleKeyPress, false);
window.addEventListener('mousedown', handleMouseDown, false);
window.addEventListener('mousewheel', handleMouseWheel, false);
window.addEventListener('drag', handleDrag, false);
// window.addEventListener('mousemove', handleMouseMove, false)
