// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests that ParseAuthorStyleSheet trace event is recorded.\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function importStyle()
      {
          var link = document.createElement('link');
          link.setAttribute('rel', 'stylesheet');
          link.type = 'text/css';
          link.href = '../resources/style.css';
          document.head.appendChild(link);
          return new Promise((fulfill) => link.onload = fulfill);
      }
  `);

  PerformanceTestRunner.invokeWithTracing('importStyle', processTracingEvents);

  function processTracingEvents() {
    var event = PerformanceTestRunner.findTimelineEvent(TimelineModel.TimelineModel.RecordType.ParseAuthorStyleSheet);
    if (event)
      TestRunner.addResult('SUCCESS: found ParseAuthorStyleSheet record');
    else
      TestRunner.addResult('FAIL: ParseAuthorStyleSheet record not found');
    TestRunner.completeTest();
  }
})();
