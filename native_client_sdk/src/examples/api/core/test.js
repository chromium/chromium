// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addTests() {
  function getNaClTimeMs() {
    return parseFloat(document.getElementById('NaCl').textContent);
  }

  function getRoundTimeMs() {
    return parseFloat(document.getElementById('Round').textContent);
  }

  function getTotalTimeMs() {
    return parseFloat(document.getElementById('Total').textContent);
  }

  function delayTest(test, delayMs) {
    test.log('Setting delay to ' + delayMs + 'ms');
    document.getElementById('delay').value = delayMs;

    test.log('Clicking start.');
    var startEl = document.getElementById('start');
    startEl.dispatchEvent(new CustomEvent('click'));

    test.log('Waiting 1 second for test to finish.');
    var intervalId = window.setInterval(function() {
      if (itrCount !== itrMax) {
        test.log('Not finished, waiting another second.');
        return;
      }

      window.clearInterval(intervalId);
      test.log('NaCl time: ' + getNaClTimeMs().toFixed(2) + 'ms');
      test.log('Roundtrip time: ' + getRoundTimeMs().toFixed(2) + 'ms');
      test.log('Total time: ' + getTotalTimeMs().toFixed(2) + 'ms');
      test.log('Finished.');
      test.pass();
    }, 1000);
  }

  common.tester.addAsyncTest('delay_0', function(test) {
    var delayMs = 0;
    delayTest(test, delayMs);
  });

  common.tester.addAsyncTest('delay_3', function(test) {
    var delayMs = 3;
    delayTest(test, delayMs);
  });
}
