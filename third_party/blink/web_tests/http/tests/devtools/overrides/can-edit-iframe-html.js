// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Ensures iframes are overridable if overrides are setup.\n`);
  await TestRunner.loadTestModule('bindings_test_runner');
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.loadLegacyModule('sources');

  var {project} = await BindingsTestRunner.createOverrideProject('file:///tmp/');
  BindingsTestRunner.setOverridesEnabled(true);

  // NOTE: localhost is considered coss-origin in this context and should be cross-origin.
  await TestRunner.addIframe('http://localhost:8000/devtools/resources/cross-origin-iframe.html');

  var uiSourceCode = Workspace.workspace.uiSourceCodes().find(uiSourceCode => uiSourceCode.url().endsWith('cross-origin-iframe.html'));
  if (!uiSourceCode)
    throw "No uiSourceCode.";
  var uiSourceCodeFrame = new Sources.UISourceCodeFrame(uiSourceCode);
  TestRunner.addResult('URL: ' + uiSourceCode.url().substr(uiSourceCode.url().lastIndexOf('/') + 1));
  TestRunner.addResult('Can Edit Source: ' + uiSourceCodeFrame.canEditSource());
  TestRunner.completeTest();
})();
