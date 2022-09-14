// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Requests the debug information from the backend.
 */
function requestOmahaDebugInformation() {
  chrome.send('requestOmahaDebugInformation');
}

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
 * Callback from backend with the debug informations. Construct the UI.
 * @param {Object} information The debug information.
 */
function updateOmahaDebugInformation(information) {
  for (key in information) {
    $(key).textContent = information[key];
    setVisible($(key + '-tr'), true);
  }
}

document.addEventListener('DOMContentLoaded', requestOmahaDebugInformation);
