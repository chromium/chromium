// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test nesting of time/timeEnd records on Timeline\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function simpleConsoleTime()
      {
          console.time("a");
          console.timeEnd("a");
      }

      function nestedConsoleTime()
      {
          console.time("a");
          {
              console.time("b");
              console.timeEnd("b");
              {
                  console.time("c");
                  {
                      console.time("d");
                      console.timeEnd("d");
                  }
                  console.timeEnd("c");
              }
          }
          console.timeEnd("a");
      }


      function unbalancedConsoleTime()
      {
          console.time("a");
          console.time("b");
          console.timeEnd("a");
          console.timeEnd("b");
      }

      function consoleTimeWithoutConsoleTimeEnd()
      {
          console.timeStamp("Foo");
          console.time("a");
          console.timeStamp("Bar");
          console.time("b");
          console.time("c");
          console.time("d");
          console.timeStamp("Baz");
          console.timeEnd("d");
      }
  `);

  TestRunner.runTestSuite([
    function testSimpleConsoleTime(next) {
      performActions('simpleConsoleTime()', next);
    },

    function testNestedConsoleTime(next) {
      performActions('nestedConsoleTime()', next);
    },

    function testUnbalancedConsoleTime(next) {
      performActions('unbalancedConsoleTime()', next);
    },

    function testConsoleTimeWithoutConsoleTimeEnd(next) {
      performActions('consoleTimeWithoutConsoleTimeEnd()', next);
    }
  ]);

  async function performActions(actions, next) {
    var namesToDump = new Set(['FunctionCall', 'ConsoleTime', 'TimeStamp']);
    function dumpName(event, level) {
      if (namesToDump.has(event.name))
        TestRunner.addResult('----'.repeat(level) + '> ' + Timeline.TimelineUIUtils.eventTitle(event));
    }
    UI.panels.timeline.disableCaptureJSProfileSetting.set(true);
    await PerformanceTestRunner.evaluateWithTimeline(actions);
    await PerformanceTestRunner.walkTimelineEventTree(dumpName);
    next();
}
})();
