// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that elements tab updates after a bf-cache navigation.\n`);
  await TestRunner.showPanel('elements');

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  const expandAndDumpTree = () => new Promise(resolve => {
    ElementsTestRunner.expandElementsTree(() => {
      ElementsTestRunner.dumpElementsTree();
      resolve();
    });
  });

  // TestRunner.navigate navigates with replacement, so we need to separately
  // define a method to navigate without replacement (need this for bf-cache
  // testing).
  // TODO(adithyas): Add this to a utils file or to TestRunner.js.
  const navigate = url => new Promise(resolve => {
    TestRunner.evaluateInPage(`window.location.href = '${url}'`);
    TestRunner.waitForPageLoad(resolve);
  });

  await TestRunner.navigatePromise('http://localhost:8000/devtools/bfcache/resources/page1.html');
  await navigate('http://devtools.oopif.test:8000/devtools/bfcache/resources/page2.html');
  await expandAndDumpTree();

  // Navigate back - uses BFCache.
  TestRunner.evaluateInPage('window.history.back()');
  await TestRunner.waitForEvent(SDK.DOMModel.Events.DocumentUpdated, TestRunner.domModel);
  await expandAndDumpTree();

  TestRunner.completeTest();
})();
