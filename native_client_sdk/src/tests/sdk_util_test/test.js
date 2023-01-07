// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addTests() {
  common.tester.addAsyncTest('sdk_util_test', function (test) {
    var intervalId = window.setInterval(function () {
      if (!testsFinished)
        return;

      window.clearInterval(intervalId);
      if (failedTests > 0)
        test.fail('tests failed');
      else
        test.pass();
    }, 100);
  });
}
