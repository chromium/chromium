// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

import * as UI from 'devtools/ui/legacy/legacy.js';
import * as Timeline from 'devtools/panels/timeline/timeline.js';

(async function() {
  TestRunner.addResult(
      `Tests that inspector doesn't force styles recalc on operations with inline element styles that result in no changes.\n`);
  await TestRunner.showPanel('timeline');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="testDiv" style="color: green">testDiv</div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var testDiv = document.querySelector("#testDiv");
          for (var i = 0; i < 20; ++i)
              testDiv.style.visibility = "";
      }
  `);

  UI.Context.Context.instance().setFlavor(Timeline.TimelinePanel.TimelinePanel, Timeline.TimelinePanel.TimelinePanel.instance());
  await PerformanceTestRunner.evaluateWithTimeline('performActions()');

  const events = PerformanceTestRunner.traceEngineRawEvents();
  if (events.length === 0) {
    TestRunner.addResult('ERROR: did not find any trace engine events.');
  }

  const updateLayoutTreeEvents = events.filter(event => event.name === 'UpdateLayoutTree');
  TestRunner.addResult(`Found ${updateLayoutTreeEvents.length} UpdateLayoutTree events (expecting 0).`);
  TestRunner.completeTest();
})();
