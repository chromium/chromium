// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Application from 'devtools/panels/application/application.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that cookies are properly shown after oopif refresh`);
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadModule('cookie_table');
  await TestRunner.showPanel('console');
  await TestRunner.showPanel('resources');

  Application.ResourcesPanel.ResourcesPanel.instance().showCookies(SDK.TargetManager.TargetManager.TargetManager.instance().primaryPageTarget(), 'http://127.0.0.1:8000');
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
