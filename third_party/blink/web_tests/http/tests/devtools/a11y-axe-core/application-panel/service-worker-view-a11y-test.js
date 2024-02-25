// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as Application from 'devtools/panels/application/application.js';

(async function() {
  TestRunner.addResult('Tests accessibility of ServiceWorkersView on application panel.');
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  const scriptURL = 'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-empty.js';
  const scope1 = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope1/';
  const scope2 = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope2/';
  Application.ServiceWorkersView.setThrottleDisabledForDebugging(true);

  Application.ResourcesPanel.ResourcesPanel.instance().sidebar.serviceWorkersTreeElement.select();
  await ApplicationTestRunner.registerServiceWorker(scriptURL, scope1);
  await ApplicationTestRunner.registerServiceWorker(scriptURL, scope2);
  const element = Application.ResourcesPanel.ResourcesPanel.instance().visibleView.contentElement;

  await AxeCoreTestRunner.runValidation(element);
  TestRunner.completeTest();
})();
