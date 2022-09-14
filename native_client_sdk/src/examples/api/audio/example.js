// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called by the common.js module.
function attachListeners() {
  document.getElementById('playButton').addEventListener('click', playSound);
  document.getElementById('stopButton').addEventListener('click', stopSound);
  document.getElementById('frequencyField').addEventListener('change',
      changeFrequency);
}

// Called by the common.js module.
function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();
}

function getFrequencyElement() {
  return document.getElementById('frequencyField');
}

function playSound() {
  common.naclModule.postMessage('setFrequency:' + getFrequencyElement().value);
  common.naclModule.postMessage('playSound');
}

function stopSound() {
  common.naclModule.postMessage('stopSound');
}

function changeFrequency() {
  common.naclModule.postMessage('setFrequency:' + getFrequencyElement().value);
}

// Called by the common.js module.
function handleMessage(e) {
  getFrequencyElement().value = message_event.data;
}
