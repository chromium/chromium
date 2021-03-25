// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests popver for spread operator.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await SourcesTestRunner.startDebuggerTestPromise(/* quiet */ true);
  TestRunner.evaluateInPageAnonymously(`
let arr = [1,2,3];
console.log(...arr);
let obj = {a:1};
console.log({...obj});
debugger;
//# sourceURL=test.js`);
  await new Promise(resolve => UI.context.addFlavorChangeListener(SDK.DebuggerModel.CallFrame, resolve));
  const sourceFrame = await SourcesTestRunner.showScriptSourcePromise('test.js');
  TestRunner.addResult('Request popover for array:');
  const {description: arr} = await SourcesTestRunner.objectForPopover(sourceFrame, 2, 16);
  TestRunner.addResult(arr);
  TestRunner.addResult('Request popover for object:');
  const {description: obj} = await SourcesTestRunner.objectForPopover(sourceFrame, 4, 17);
  TestRunner.addResult(obj);
  SourcesTestRunner.completeDebuggerTest();
})();
