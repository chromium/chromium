// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests resource tree model on iframe addition, compares resource tree against golden. Every line is important.\n`);
  await TestRunner.showPanel('resources');
  await TestRunner.addStylesheetTag('resources/styles-initial.css');

  TestRunner.addResult('Before addition');
  TestRunner.addResult('====================================');
  ApplicationTestRunner.dumpResourceTreeEverything();
  TestRunner.evaluateInPageAnonymously(`
    (function createIframe() {
      var iframe = document.createElement("iframe");
      iframe.src = "${TestRunner.url('resources/resource-tree-frame-add-iframe.html')}";
      document.body.appendChild(iframe);
    })();
  `);

  await Promise.all([
    TestRunner.waitForUISourceCode('styles-navigated.css'),
    TestRunner.waitForUISourceCode('script-navigated.js'),
    TestRunner.waitForUISourceCode('resource-tree-frame-add-iframe.html'),
  ]);
  TestRunner.addResult('');
  TestRunner.addResult('After addition');
  TestRunner.addResult('====================================');
  ApplicationTestRunner.dumpResourceTreeEverything();
  TestRunner.completeTest();
})();
