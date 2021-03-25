// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test editor cursorPositionToCoordinates and coordinatesToCursorPosition API\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  var text = ['function foo(a, b) {', '    var f = /*.[a]/.test(a);', '    return f;'];
  var textEditor = SourcesTestRunner.createTestEditor();
  textEditor.setText(text.join('\n'));

  TestRunner.runTestSuite([
    function testCoordinatesToCursorPositionOuter(next) {
      TestRunner.addResult('Request char at (-1000, -1000)');
      TestRunner.addResult('Result:' + JSON.stringify(textEditor.coordinatesToCursorPosition(-1000, -1000)));
      next();
    },

    function testTextToCoordinatesCornerCases(next) {
      TestRunner.addResult('Request (-1, 0):' + JSON.stringify(textEditor.cursorPositionToCoordinates(-1, 0)));
      TestRunner.addResult('Request (100, 0):' + JSON.stringify(textEditor.cursorPositionToCoordinates(100, 0)));
      TestRunner.addResult('Request (0, -1):' + JSON.stringify(textEditor.cursorPositionToCoordinates(0, -1)));
      TestRunner.addResult('Request (0, 100):' + JSON.stringify(textEditor.cursorPositionToCoordinates(0, 100)));
      TestRunner.addResult('Request (-100, -1100):' + JSON.stringify(textEditor.cursorPositionToCoordinates(0, 100)));
      next();
    },

    function testInverseRelationTextToCoordinates(next) {
      for (var i = 0; i < textEditor.linesCount; ++i) {
        var line = textEditor.line(i);
        TestRunner.addResult('Testing line \'' + line + '\'');
        for (var j = 0; j < textEditor.length; ++j) {
          var xy = textEditor.cursorPositionToCoordinates(i, j);
          if (!xy) {
            TestRunner.addResult('Failed inversion for line=' + i + ' column=' + j);
            continue;
          }

          var range = textEditor.coordinatesToCursorPosition(xy.x, xy.y);
          if (range.startLine !== i || range.startColumn !== j)
            TestRunner.addResult('Failed inversion for line=' + i + ' column=' + j);
        }
      }
      next();
    },
  ]);
})();
