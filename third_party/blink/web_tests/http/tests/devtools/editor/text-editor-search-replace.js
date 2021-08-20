// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the search replace functionality.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('../sources/debugger/resources/edit-me.js');

  var textEditor;
  var panel = UI.panels.sources;
  SourcesTestRunner.showScriptSource('edit-me.js', didShowScriptSource);

  function showReplaceField() {
    var searchableView = UI.panels.sources.searchableView();
    searchableView.showSearchField();
    searchableView._replaceToggleButton.setToggled(true);
    searchableView._updateSecondRowVisibility();
  }

  function runReplaceAll(searchValue, replaceValue) {
    panel.searchableView()._searchInputElement.value = searchValue;
    panel.searchableView()._replaceInputElement.value = replaceValue;
    panel.searchableView()._replaceAll();
  }

  function dumpTextEditor(message) {
    TestRunner.addResult(message);
    TestRunner.addResult(textEditor.text());
  }

  function didShowScriptSource(sourceFrame) {
    textEditor = sourceFrame._textEditor;
    showReplaceField();

    TestRunner.runTestSuite([
      function testReplaceAll(next) {
        var source = '// var a1, a2, a3;\nconst  a1, a2, a3;\n';
        sourceFrame.setContent(source);

        dumpTextEditor('--- Before replace ---');

        runReplaceAll('a1', 'a$$');
        runReplaceAll('a2', 'b$&');
        runReplaceAll('a3', 'a3 /* $0 $1 $2 $& $$ \\0 \\1 */');
        runReplaceAll('/\\b(const)(\\s)+/', '/** @$1 */ var$2');
        runReplaceAll('//', '//=');

        dumpTextEditor('--- After replace ---');

        next();
      },
    ]);
  }
})();
