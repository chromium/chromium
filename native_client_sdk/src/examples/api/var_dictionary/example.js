// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function attachListeners() {
  var radioEls = document.querySelectorAll('input[type="radio"]');
  for (var i = 0; i < radioEls.length; ++i) {
    radioEls[i].addEventListener('click', onRadioClicked);
  }

  // Wire up the 'click' event for each function's button.
  var functionEls = document.querySelectorAll('.function');
  for (var i = 0; i < functionEls.length; ++i) {
    var functionEl = functionEls[i];
    var id = functionEl.getAttribute('id');
    var buttonEl = functionEl.querySelector('button');

    // The function name matches the element id.
    var func = window[id + 'ButtonClicked'];
    buttonEl.addEventListener('click', func);
  }
}

function onRadioClicked(e) {
  var divId = this.id.slice(5);  // skip "radio"
  var functionEls = document.querySelectorAll('.function');
  for (var i = 0; i < functionEls.length; ++i) {
    var visible = functionEls[i].id === divId;
    if (functionEls[i].id === divId)
      functionEls[i].removeAttribute('hidden');
    else
      functionEls[i].setAttribute('hidden', '');
  }
}

function getButtonClicked(e) {
  var key = document.getElementById('getKey').value;
  common.naclModule.postMessage({cmd: 'Get', key: key});
}

function setButtonClicked(e) {
  var key = document.getElementById('setKey').value;
  var valueString = document.getElementById('setValue').value;
  var value;
  try {
    value = JSON.parse(valueString);
  } catch (e) {
    common.logMessage('Error parsing value: ' + e);
    return;
  }

  common.naclModule.postMessage({cmd: 'Set', key: key, value: value});
}

function deleteButtonClicked(e) {
  var key = document.getElementById('deleteKey').value;
  common.naclModule.postMessage({cmd: 'Delete', key: key});
}

function getkeysButtonClicked(e) {
  common.naclModule.postMessage({cmd: 'GetKeys'});
}

function haskeyButtonClicked(e) {
  var key = document.getElementById('haskeyKey').value;
  common.naclModule.postMessage({cmd: 'HasKey', key: key});
}

// Called by the common.js module.
function handleMessage(message_event) {
  var msg = message_event.data;

  var cmd = msg.cmd;
  var result = msg.result;
  var newDict = msg.dict;

  // cmd will be empty when the module first loads and sends the contents of
  // its dictionary.
  //
  // Note that cmd is a String object, not a primitive, so the comparison cmd
  // !== '' will always fail.
  if (cmd != '') {
    common.logMessage(
        'Function ' + cmd + ' returned ' + JSON.stringify(result));
  }

  var dictEl = document.getElementById('dict');
  dictEl.textContent = JSON.stringify(newDict, null, '  ');
}
