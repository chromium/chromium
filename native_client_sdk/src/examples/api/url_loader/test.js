// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addTests() {
  common.tester.addAsyncTest('get_url', function(test) {
    test.log('Clicking the button');
    document.getElementById('button').dispatchEvent(new MouseEvent('click'));

    var outputEl = document.getElementById('output');
    outputEl.textContent = '';

    test.log('Waiting for the URL to load.');
    var intervalId = window.setInterval(function() {
      if (!outputEl.textContent)
        return;

      window.clearInterval(intervalId);
      test.log('Output box changed...');
      var expectedMessage = 'part of the test output';
      if (outputEl.textContent.indexOf(expectedMessage) === -1) {
        test.fail('Expected to find "' + expectedMessage + '" in the output,' +
                  'instead got "' + outputEl.textContent + '"');
        return;
      } else {
        test.log('OK, found "' + expectedMessage + '".');
      }

      test.pass();
    }, 100);
  });
}
