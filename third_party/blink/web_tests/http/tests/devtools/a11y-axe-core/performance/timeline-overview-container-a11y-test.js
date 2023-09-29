// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult('Testing a11y in performance panel - timeline overview container.');

  await TestRunner.showPanel('timeline');

  await PerformanceTestRunner.runPerfTraceWithReload();
  const widget = await PerformanceTestRunner.getTimelineWidget();
  const timeLineViewElement = widget.overviewPane.element;
  await AxeCoreTestRunner.runValidation(timeLineViewElement);

  TestRunner.completeTest();
})();
