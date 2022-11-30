// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called by the common.js module.
function attachListeners() {
  document.querySelector('form').addEventListener('submit', askBall);
  document.getElementById('reverse').addEventListener('click', reverseString);
}

// Called by the common.js module.
function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();
}

function askBall(event) {
  var questionEl = document.getElementById('question');
  var query = questionEl.value;
  questionEl.value = '';
  common.logMessage('You asked: ' + query);
  common.naclModule.postMessage('eightball');
  event.preventDefault();
}

function reverseString(event) {
  var questionEl = document.getElementById('question');
  var query = questionEl.value;
  questionEl.value = '';
  common.logMessage('Reversing: ' + query);
  common.naclModule.postMessage('reverse:' + query);
}
