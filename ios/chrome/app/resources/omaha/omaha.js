// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/ios/web_ui.js';

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util.js';


/**
 * Update the visibility state of the given element. Using the hidden attribute
 * is not supported on iOS 4.3.
 * @param {Element} element The element to update.
 * @param {boolean} visible The new visibility state.
 */
function setVisible(element, visible) {
  element.style.display = visible ? 'inline' : 'none';
}

/**
 * Callback with the debug information. Construct the UI.
 * @param {Object} information The debug information.
 */
function updateOmahaDebugInformation(information) {
  for (const key in information) {
    $(key).textContent = information[key];
    setVisible($(key + '-tr'), true);
  }
}

document.addEventListener('DOMContentLoaded', async () => {
  const information = await sendWithPromise('requestOmahaDebugInformation');
  updateOmahaDebugInformation(information);
});
