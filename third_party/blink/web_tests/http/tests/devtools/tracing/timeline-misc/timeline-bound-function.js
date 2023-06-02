// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests extracting information about original functions from bound ones\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <button id="btn"></button>
    `);
  await TestRunner.evaluateInPagePromise(`
      function original() { }

      function performActions()
      {
          var b = document.getElementById("btn");
          var foo = original.bind();
          b.onclick = foo;
          b.click();
      }
  `);

  await PerformanceTestRunner.evaluateWithTimeline('performActions()');

  PerformanceTestRunner.mainTrackEvents().forEach(event => {
    if (event.name !== TimelineModel.TimelineModel.RecordType.FunctionCall)
      return;
    var data = event.args['data'];
    var scriptName = data.scriptName;
    var scriptNameShort = scriptName.substring(scriptName.lastIndexOf('/') + 1);
    TestRunner.addResult(`${event.name} ${scriptNameShort}: ${data.scriptLine}`);
  });
  TestRunner.completeTest();
})();
