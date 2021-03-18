// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test editor tokenAtTextPosition method.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadLegacyModule('source_frame');
  await TestRunner.showPanel('sources');

  var text =
      ['function foo(a, b) {', '    var f = /.a/.test(a); /*', 'this is a comment */', '    return f + "looongword";'];

  var positions = [[0, 10, 13], [8, 14, 19], [0, 5], [5, 20]];
  function testTokenAtPosition(textEditor) {
    for (var i = 0; i < positions.length; ++i) {
      var columns = positions[i];
      TestRunner.addResult('Line: ' + text[i]);
      for (var j = 0; j < columns.length; ++j) {
        var column = columns[j];
        TestRunner.addResult(
            'Column #' + column + ' (char \'' + text[i].charAt(column) +
            '\') - token: ' + JSON.stringify(textEditor.tokenAtTextPosition(i, column)));
      }
    }
  }

  TestRunner.runTestSuite([
    function testHighlightedText(next) {
      var textEditor = SourcesTestRunner.createTestEditor();
      TestRunner.addSnifferPromise(SourceFrame.SourcesTextEditor.prototype, 'rewriteMimeType').then(step1);
      textEditor.setMimeType('text/javascript');
      function step1() {
        textEditor.setText(text.join('\n'));
        testTokenAtPosition(textEditor);
        next();
      }
    },
  ]);
})();
