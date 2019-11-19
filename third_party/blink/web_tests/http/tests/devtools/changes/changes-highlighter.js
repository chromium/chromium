// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the changes view highlights diffs correctly.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('changes');
  await TestRunner.showPanel('sources');
  await TestRunner.addStylesheetTag('resources/before.css');
  await TestRunner.addStylesheetTag('resources/after.css');

  TestRunner.waitForUISourceCode('after.css').then(uiSourceCode => uiSourceCode.requestContent()).then(onAfterContent);

  function onAfterContent({ content, error, isEncoded }) {
    SourcesTestRunner.waitForScriptSource('before.css', uiSourceCode => uiSourceCode.setWorkingCopy(content));
    TestRunner.addSniffer(Changes.ChangesView.prototype, '_renderDiffRows', rowsRendered, true);
    UI.viewManager.showView('changes.changes');
  }

  function rowsRendered() {
    var codeMirror = this._editor._codeMirror;
    for (var i = 0; i < codeMirror.lineCount(); i++) {
      codeMirror.scrollIntoView(i);  // Ensure highlighting
      var lineInfo = codeMirror.lineInfo(i);
      var prefix = '';
      if (lineInfo.handle.styleClasses.bgClass === 'deletion')
        prefix = '-';
      else if (lineInfo.handle.styleClasses.bgClass === 'addition')
        prefix = '+';
      else if (lineInfo.handle.styleClasses.bgClass === 'equal')
        prefix = ' ';
      TestRunner.addResult(prefix + ' ' + lineInfo.text);
    }
    TestRunner.completeTest();
  }
})();
