// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Diff from "devtools/third_party/diff/diff.js";

(async function() {
  TestRunner.addResult(`Tests that the Diff module correctly diffs things.\n`);

  print(Diff.Diff.DiffWrapper.charDiff('test this sentence.', 'test that sentence'));
  print(Diff.Diff.DiffWrapper.lineDiff(['test this sentence.'], ['test that sentence']));
  print(Diff.Diff.DiffWrapper.lineDiff(['a', 'b', 'c'], ['a', 'c']));
  print(Diff.Diff.DiffWrapper.lineDiff(['a', 'b', 'c'], ['b', 'a', 'c']));
  print(Diff.Diff.DiffWrapper.lineDiff(['a', 'c'], ['a', 'b', 'c']));
  print(Diff.Diff.DiffWrapper.lineDiff(
      [
        'for (var i = 0; i < 100; i++) {', '    willBeLeftAlone()', '    willBeDeleted();', '}',
        'for (var j = 0; j < 100; j++) {', '    console.log(\'something changed\');', '    willBeDeletedAgain();', '}'
      ],
      [
        'for (var i = 0; i < 100; i++) {', '    willBeLeftAlone();', '}', 'insertThisLine();',
        'for (var j = 0; j < 100; j++) {', '    console.log(\'changed\');', '}'
      ]));
  TestRunner.completeTest();
  function print(results) {
    TestRunner.addResult('');
    for (var i = 0; i < results.length; i++) {
      var result = results[i];
      var type = 'Unknown';
      if (result[0] === Diff.Diff.Operation.Equal)
        type = '=';
      else if (result[0] === Diff.Diff.Operation.Insert)
        type = '+';
      else if (result[0] === Diff.Diff.Operation.Delete)
        type = '-';
      else if (result[0] === Diff.Diff.Operation.Edit)
        type = 'E';
      TestRunner.addResult(type + ': ' + JSON.stringify(result[1], null, 4));
    }
  }
})();
