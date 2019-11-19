// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that SecurityStateComparator correctly compares the severity of security states.\n`);
  await TestRunner.loadModule('security_test_runner');
  await TestRunner.showPanel('security');

  var ordering = [
    Protocol.Security.SecurityState.Info, Protocol.Security.SecurityState.InsecureBroken,
    Protocol.Security.SecurityState.Insecure, Protocol.Security.SecurityState.Neutral,
    Protocol.Security.SecurityState.Secure, Protocol.Security.SecurityState.Unknown
  ];

  TestRunner.assertEquals(ordering.length, Object.keys(Protocol.Security.SecurityState).length);

  for (var i = 0; i < ordering.length; i++) {
    TestRunner.assertEquals(
        Security.SecurityModel.SecurityStateComparator(ordering[i], ordering[i]), 0,
        'Security state comparison failed when checking that "' + ordering[i] + '" == "' + ordering[i] + '"');
  }

  for (var i = 0; i < ordering.length; i++) {
    var j;

    for (j = 0; j < i; j++) {
      TestRunner.addResult(
          'Sign of SecurityStateComparator("' + ordering[i] + '","' + ordering[j] + '"): ' +
          Math.sign(Security.SecurityModel.SecurityStateComparator(ordering[i], ordering[j])) + ' (expected: 1)');
    }

    TestRunner.addResult(
        'Sign of SecurityStateComparator("' + ordering[i] + '","' + ordering[j] + '"): ' +
        Math.sign(Security.SecurityModel.SecurityStateComparator(ordering[i], ordering[j])) + ' (expected: 0)');

    for (j = i + 1; j < ordering.length; j++) {
      TestRunner.addResult(
          'Sign of SecurityStateComparator("' + ordering[i] + '","' + ordering[j] + '"): ' +
          Math.sign(Security.SecurityModel.SecurityStateComparator(ordering[i], ordering[j])) + ' (expected: -1)');
    }
  }

  TestRunner.completeTest();
})();
