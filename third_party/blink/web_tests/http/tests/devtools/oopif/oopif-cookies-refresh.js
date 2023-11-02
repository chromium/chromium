// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that cookies are properly shown after oopif refresh`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadModule('cookie_table');
  await TestRunner.showPanel('console');
  await TestRunner.showPanel('resources');

  UI.panels.resources.showCookies(SDK.targetManager.mainTarget(), 'http://127.0.0.1:8000');
  await ApplicationTestRunner.waitForCookies();

  await TestRunner.navigatePromise('resources/page-out.html');
  await TestRunner.evaluateInPageAsync(`document.cookie = 'test=cookie;'`);
  await TestRunner.addIframe('http://devtools.oopif.test:8000/devtools/oopif/resources/inner-iframe.html');
  await TestRunner.reloadPagePromise();
  await TestRunner.addIframe('http://devtools.oopif.test:8000/devtools/oopif/resources/inner-iframe.html');
  await ApplicationTestRunner.waitForCookies();

  ApplicationTestRunner.dumpCookieDomains();
  ApplicationTestRunner.dumpCookies();
  TestRunner.completeTest();
})();