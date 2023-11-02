// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a TimerFired events inside evaluated scripts.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
function performActions()
{
    var promise = new Promise((fulfill) => window.callWhenDone = fulfill);
    var content = "" +
        "var fn2 = function() {" +
        "    console.timeStamp(\\"Script evaluated\\");" +
        "    window.callWhenDone();" +
        "};\\\\n" +
        "var fn1 = function() {" +
        "    window.setTimeout(fn2, 1);" +
        "};\\\\n" +
        "window.setTimeout(fn1, 1);\\\\n" +
        "//# sourceURL=fromEval.js";
    content = "eval('" + content + "');";
    var scriptElement = document.createElement('script');
    var contentNode = document.createTextNode(content);
    scriptElement.appendChild(contentNode);
    document.body.appendChild(scriptElement);
    document.body.removeChild(scriptElement);
    return promise;
}
`);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  var events = PerformanceTestRunner.mainTrackEvents();
  for (var i = 0; i < events.length; ++i) {
    if (events[i].name !== TimelineModel.TimelineModel.RecordType.TimerFire)
      continue;
    var functionCallChild =
        PerformanceTestRunner.findChildEvent(events, i, TimelineModel.TimelineModel.RecordType.FunctionCall);
    var fnCallSite = functionCallChild.args['data'];
    TestRunner.addResult(`${events[i].name} ${fnCallSite.url}:${fnCallSite.lineNumber + 1}`);
  }
  TestRunner.completeTest();
})();
