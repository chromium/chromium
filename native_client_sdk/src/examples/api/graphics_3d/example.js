// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function $(id) {
  return document.getElementById(id);
}

function postAngleMessage() {
  var xAngle = parseFloat($('xAngle').value);
  var yAngle = parseFloat($('yAngle').value);
  common.naclModule.postMessage([xAngle, yAngle]);
}

// Add event listeners after the NaCl module has loaded.  These listeners will
// forward messages to the NaCl module via postMessage()
function attachListeners() {
  $('xAngle').addEventListener('change', postAngleMessage);
  $('yAngle').addEventListener('change', postAngleMessage);
  $('animateOff').addEventListener('click', function() {
    $('animateOn').checked = '';
    common.naclModule.postMessage(false);
  });
  $('animateOn').addEventListener('click', function() {
    $('animateOff').checked = '';
    common.naclModule.postMessage(true);
  });
}

// Handle a message coming from the NaCl module.
function handleMessage(event) {
  if (!(event.data instanceof Array))
    return;
  if (event.data.length != 2)
    return;

  var xAngle = event.data[0];
  var yAngle = event.data[1];
  $('xAngle').value = xAngle;
  $('yAngle').value = yAngle;
}
