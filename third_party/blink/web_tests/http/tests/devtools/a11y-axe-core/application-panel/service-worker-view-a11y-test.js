// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests accessibility of ServiceWorkersView on application panel.`);
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('application_test_runner');
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  const scriptURL = 'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-empty.js';
  const scope1 = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope1/';
  const scope2 = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope2/';
  Resources.ServiceWorkersView._noThrottle = true;

  UI.panels.resources._sidebar.serviceWorkersTreeElement.select();
  ApplicationTestRunner.registerServiceWorker(scriptURL, scope1);
  ApplicationTestRunner.registerServiceWorker(scriptURL, scope2);
  const element = UI.panels.resources.visibleView.contentElement;

  await AxeCoreTestRunner.runValidation(element);
  TestRunner.completeTest();
})();