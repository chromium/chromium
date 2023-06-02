// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Test the Timeline instrumentation contains Profile event.`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');

  await PerformanceTestRunner.evaluateWithTimeline('42');
  const profileEvent = PerformanceTestRunner.findTimelineEvent(TimelineModel.TimelineModel.RecordType.Profile);
  TestRunner.check(profileEvent, 'Profile trace event not found.');

  TestRunner.completeTest();
})();
