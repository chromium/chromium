// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as Application from 'devtools/panels/application/application.js';

(async function() {
  TestRunner.addResult(`Verify DOM storage with OOPIFs`);
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();
  await TestRunner.navigatePromise('resources/page.html');
  await TestRunner.showPanel('resources');

  const localStorageTree = Application.ResourcesPanel.ResourcesPanel.instance()
                               .sidebar.localStorageListTreeElement;
  const sessionStorageTree =
      Application.ResourcesPanel.ResourcesPanel.instance()
          .sidebar.sessionStorageListTreeElement;

  if (localStorageTree.childCount() < 2 ||
      sessionStorageTree.childCount() < 2) {
    await new Promise(resolve => {
      const check = () => {
        if (localStorageTree.childCount() >= 2 &&
            sessionStorageTree.childCount() >= 2) {
          resolve();
        }
      };
      TestRunner.addSniffer(localStorageTree, 'appendChild', check, true);
      TestRunner.addSniffer(sessionStorageTree, 'appendChild', check, true);
    });
  }

  localStorageTree.expandRecursively(1000);
  sessionStorageTree.expandRecursively(1000);

  TestRunner.addResult('Local Storage:');
  TestRunner.addResult(localStorageTree.childrenListElement.deepTextContent());

  TestRunner.addResult('Session Storage:');
  TestRunner.addResult(
      sessionStorageTree.childrenListElement.deepTextContent());

  TestRunner.completeTest();
})();
