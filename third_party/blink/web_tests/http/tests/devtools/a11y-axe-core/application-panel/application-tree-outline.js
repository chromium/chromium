// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';

import * as Application from 'devtools/panels/application/application.js';

(async function() {

  TestRunner.addResult('Tests accessibility of Tree outline sidepane in Application Panel.');

  await TestRunner.showPanel('resources');
  const applicationSidebar = Application.ResourcesPanel.ResourcesPanel.instance().panelSidebarElement();
  await AxeCoreTestRunner.runValidation(applicationSidebar);
  TestRunner.completeTest();
})();
