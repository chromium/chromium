// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function attachListeners() {
  var number1El = document.querySelector('#addend1');
  var number2El = document.querySelector('#addend2');
  var resultEl = document.querySelector('#result');

  document.getElementById('addAsync').addEventListener('click', function() {
    var value1 = parseInt(number1El.value);
    var value2 = parseInt(number2El.value);
    common.naclModule.postMessage([value1, value2]);

    // The result is returned in handleMessage below.
  });

  document.getElementById('addSync').addEventListener('click', function() {
    var value1 = parseInt(number1El.value);
    var value2 = parseInt(number2El.value);
    var result =
        common.naclModule.postMessageAndAwaitResponse([value1, value2]);

    // This is the result returned from the module synchronously (i.e. when the
    // addSync button is pressed)
    resultEl.textContent = result;
  });
}

// Called by the common.js module.
function handleMessage(message_event) {
  // This is the result returned from the module asynchronously (i.e. when the
  // addAsync button is pressed)
  result.textContent = message_event.data;
}
