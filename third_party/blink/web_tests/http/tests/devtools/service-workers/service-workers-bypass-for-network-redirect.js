// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Tests "Bypass for network" checkbox with redirection doesn't cause crash.\n`);
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  const url = 'http://localhost:8000/devtools/service-workers/resources/' +
      'bypass-for-network-redirect.php';
  const frameId = 'frame_id';

  UI.InspectorView.InspectorView.instance().showPanel('sources')
      .then(function() {
        Common.Settings.settingForTest('bypass-service-worker').set(true);
        let callback;
        const promise = new Promise((fulfill) => callback = fulfill);
        ConsoleTestRunner.addConsoleSniffer(message => {
          if (message.messageText == 'getRegistration finished') {
            callback();
          }
        }, true);
        TestRunner.addIframe(url, {id: frameId});
        return promise;
      })
      .then(function() {
        TestRunner.addResult('Success');
        TestRunner.completeTest();
      })
      .catch(function(exception) {
        TestRunner.addResult('Error');
        TestRunner.addResult(exception);
        TestRunner.completeTest();
      });
})();
