// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that script is replaced with the newer version when the names match.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function injectScript(value)
      {
          eval("function foo() { return " + value + "; } //# sourceURL=MyScript.js");
      }
  `);

  TestRunner.evaluateInPage('injectScript(1);');
  TestRunner.evaluateInPage('injectScript(2);');

  Workspace.workspace.addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, reportAdded);
  Workspace.workspace.addEventListener(Workspace.Workspace.Events.UISourceCodeRemoved, reportRemoved);

  var iteration = 0;

  function reportAdded(event) {
    if (event.data.url().indexOf('MyScript.js') === -1)
      return;
    TestRunner.addResult(
        'Added: ' + event.data.url().replace(/VM[\d]+/, 'VMXX') + ' to ' + event.data.project().type());
    if (event.data.project().type() !== 'network')
      return;
    event.data.requestContent().then(function(it, content) {
      TestRunner.addResult('Content: ' + content.content);
      if (it)
        TestRunner.completeTest();
    }.bind(null, iteration++));
  }

  function reportRemoved(event) {
    if (event.data.url() !== 'MyScript.js')
      return;
    TestRunner.addResult('Removed: ' + event.data.url() + ' from ' + event.data.project().type());
  }
})();
