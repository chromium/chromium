// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests async call stack for workers.\n`);
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(
      `
    var response = ` +
      '`' +
      `
    postMessage('ready');
    self.onmessage=function(e){
      debugger;
    }
    //# sourceURL=worker.js` +
      '`' +
      `;
    var blob = new Blob([response], {type: 'application/javascript'});
    function testFunction() {
      var worker = new Worker(URL.createObjectURL(blob));
      worker.onmessage = function(e) {
        worker.postMessage(42);
      };
    }`);

  SourcesTestRunner.startDebuggerTestPromise(/* quiet */ true)
      .then(() => SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise())
      .then(
          () => TestRunner.addSnifferPromise(
              Sources.CallStackSidebarPane.prototype, 'updatedForTest'))
      .then(() => dumpCallStackSidebarPane())
      .then(() => SourcesTestRunner.completeDebuggerTest());

  function dumpCallStackSidebarPane() {
    var pane = Sources.CallStackSidebarPane.instance();
    for (var element of pane.contentElement.querySelectorAll(
             '.call-frame-item'))
      TestRunner.addResult(element.deepTextContent()
                               .replace(/VM\d+/g, 'VM')
                               .replace(/blob:http:[^:]+/, 'blob'));
  }
})();
