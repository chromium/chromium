// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that php highlighter loads successfully.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadLegacyModule('source_frame');
  await TestRunner.showPanel('sources');

  var mimeType = 'text/x-php';
  var textEditor = SourcesTestRunner.createTestEditor();
  TestRunner.addSnifferPromise(SourceFrame.SourcesTextEditor.prototype, 'rewriteMimeType').then(onModesLoaded);
  textEditor.setMimeType(mimeType);
  function onModesLoaded() {
    TestRunner.addResult('Mode loaded: ' + !!CodeMirror.mimeModes[mimeType]);
    TestRunner.completeTest();
  }
})();
