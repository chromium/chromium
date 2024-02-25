// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Tests that script is replaced with the newer version when the names match.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function injectScript(value)
      {
          eval("function foo() { return " + value + "; } //# sourceURL=MyScript.js");
      }
  `);

  TestRunner.evaluateInPage('injectScript(1);');
  TestRunner.evaluateInPage('injectScript(2);');

  Workspace.Workspace.WorkspaceImpl.instance().addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, reportAdded);
  Workspace.Workspace.WorkspaceImpl.instance().addEventListener(Workspace.Workspace.Events.UISourceCodeRemoved, reportRemoved);

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
