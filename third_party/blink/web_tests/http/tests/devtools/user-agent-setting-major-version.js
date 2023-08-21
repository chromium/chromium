// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Network from 'devtools/panels/network/network.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Test user agent setting\n`);
  await TestRunner.showPanel('network');

  const chromeRegex = new RegExp('(?:^|\\W)Chrome/(\\S+)');
  const chromeUserAgentVersion = navigator.userAgent.match(chromeRegex)[1];
  const additionalAppVersion = chromeUserAgentVersion.split('.', 1)[0] + '.0.100.0';

  TestRunner.addResult('Detected Chrome user agent version: ' + chromeUserAgentVersion);
  TestRunner.addResult('Generated app version: ' + additionalAppVersion);

  for (const userAgentDescriptor of Network.NetworkConfigView.userAgentGroups) {
    for (const userAgentVersion of userAgentDescriptor.values) {

      function failTest(reason) {
        TestRunner.addResult(userAgentVersion.title + ': FAILED TEST because ' + reason);
        TestRunner.addResult('=== DO NOT COMMIT THIS INTO -expected.txt ===');
        TestRunner.completeTest();
      }

      // Split the original user agent string by %s
      //  If the returned array has length === 1, then no subsitutions will occur
      //  If the returned array has length === 2, then Chrome user agent version is substituted
      //  If the returned array has length === 3, then validate Chrome user agent version, and
      //                                          the generated app version are subsituted (in that order)
      //  Otherwise fail the test
      const splitUserAgentVersion = userAgentVersion.value.split('%s');

      if (splitUserAgentVersion.length > 3)
        failTest('Too many %s in user agent string.');

      let testPatchedUserAgentVersion = splitUserAgentVersion[0];

      if (splitUserAgentVersion.length >= 2) {
        // The first split user agent must end with either Chrome/ or CriOS/
        if (!(testPatchedUserAgentVersion.endsWith('Chrome/') || testPatchedUserAgentVersion.endsWith('CriOS/')))
          failTest('First %s match was not prefixed with either Chrome/ or CriOS/');

        testPatchedUserAgentVersion += chromeUserAgentVersion + splitUserAgentVersion[1];
      }

      if (splitUserAgentVersion.length === 3)
          testPatchedUserAgentVersion += additionalAppVersion + splitUserAgentVersion[2];

      const patchedUserAgentVersion = SDK.NetworkManager.MultitargetNetworkManager.patchUserAgentWithChromeVersion(userAgentVersion.value);

      if (patchedUserAgentVersion !== testPatchedUserAgentVersion)
          failTest('Computed user agent strings are not equal.');
      else
        TestRunner.addResult(userAgentVersion.title + ': PASSED');
    }
  }

  TestRunner.completeTest();
})();
