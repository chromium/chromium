// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addTests() {
  var currentTest = null;

  function dispatchClick(id) {
    currentTest.log('Clicking button "' + id + '".');
    $(id).dispatchEvent(new MouseEvent('click'));
  }

  function setInputValue(id, value) {
    currentTest.log('Setting input box "' + id + '" to "' + value + '".');
    $(id).value = value;
  }

  function clearResult() {
    $('result').textContent = '';
  }

  function getResult() {
    return $('result').textContent;
  }

  function waitForResult(expected) {
    var intervalId = window.setInterval(function() {
      var actual = parseInt(getResult());

      if (result === '') {
        currentTest.log('No result yet, waiting.');
        return;
      }

      // Got a result.
      window.clearInterval(intervalId);

      if (actual === expected) {
        currentTest.log('Got expected value (' + expected + ').');
        currentTest.pass();
      } else {
        currentTest.fail('Unexpected value ' + actual + ', expected ' +
                         expected);
      }
    }, 100);
  }

  common.tester.addAsyncTest('async_message', function(test) {
    currentTest = test;
    clearResult();
    setInputValue('addend1', 1234);
    setInputValue('addend2', 2345);
    dispatchClick('addAsync');
    waitForResult(3579);
  });

  common.tester.addAsyncTest('sync_message', function(test) {
    currentTest = test;
    clearResult();
    setInputValue('addend1', 42);
    setInputValue('addend2', 314);
    dispatchClick('addSync');
    waitForResult(356);
  });
}
