// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as TextUtils from 'devtools/models/text_utils/text_utils.js';

(async function() {
  TestRunner.addResult(`Test TextUtils.TextUtils.BalancedJSONTokenizer.\n`);

  const BalancedJSONTokenizer = TextUtils.TextUtils.BalancedJSONTokenizer ||
      TextUtils.TextUtils.Utils.BalancedJSONTokenizer;

  TestRunner.runTestSuite([
    function testMatchQuotes(next) {
      var testStrings = [
        {'odd back slashes with text around': 'tes\\"t'}, {'escaped double quotes': '"test"'},
        {'escaped back slash before double quote': 'test\\'}, {1: 2}, {'': ''}, {'nested brackets': {}},
        {'nested brackets with double quotes': {'': ''}}, {'etc': {'\\': '"'}}, {'etc': {'\\\\': '\\'}},
        {'etc': {'\\\\"': '\\\\"'}}
      ];

      for (var i = 0; i < testStrings.length; ++i) {
        var string = JSON.stringify(testStrings[i]);
        TestRunner.addResult('\nParsing ' + string);
        var tokenizer =
            new BalancedJSONTokenizer(TestRunner.addResult.bind(TestRunner));
        var result = tokenizer.write(string);
        if (!result)
          TestRunner.addResult(`tokenizer.write() returned ${result}, true expected`);
      }
      next();
    },

    function testMatchSequenceUsingOneShot(next) {
      var testData = [
        {'one': 'one'},
        [{'one': 'one'}, {'two': 'two'}],
        [{'one': 'one'}, {'two': 'two'}, {'three': 'three'}],
      ];

      for (var i = 0; i < testData.length; ++i) {
        var string = JSON.stringify(testData[i]);
        TestRunner.addResult('\nParsing ' + string);
        var tokenizer =
            new BalancedJSONTokenizer(TestRunner.addResult.bind(TestRunner));
        var result = tokenizer.write(string);
        if (!result)
          TestRunner.addResult(`tokenizer.write() returned ${result}, false expected`);
      }
      next();
    },

    function testMatchSequenceUsingMultiple(next) {
      var testData = [
        {'one': 'one'},
        [{'one': 'one'}, {'two': 'two'}],
        [{'one': 'one'}, {'two': 'two'}, {'three': 'three'}],
      ];

      for (var i = 0; i < testData.length; ++i) {
        var string = JSON.stringify(testData[i]);
        TestRunner.addResult('\nParsing ' + string);
        var tokenizer = new BalancedJSONTokenizer(
            TestRunner.addResult.bind(TestRunner), true);
        var result = tokenizer.write(string);
        var expectedResult = !(testData[i] instanceof Array);
        if (result != expectedResult)
          TestRunner.addResult(`tokenizer.write() returned ${result}, ${expectedResult} expected`);
      }
      next();
    },

    function testIncrementalWrites(next) {
      var testStrings = [
        {'odd back slashes with text around': 'tes\\"t'}, {'escaped double quotes': '"test"'},
        {'escaped back slash before double quote': 'test\\'}, {1: 2}, {'': ''}, {'nested brackets': {}},
        {'nested brackets with double quotes': {'': ''}}, {'etc': {'\\': '"'}}, {'etc': {'\\\\': '\\'}},
        {'etc': {'\\\\"': '\\\\"'}}
      ];
      var string = JSON.stringify(testStrings);
      var tokenizer = new BalancedJSONTokenizer(
          TestRunner.addResult.bind(TestRunner), true);
      TestRunner.addResult('\nRunning at once:');
      var result = tokenizer.write(string);
      if (result)
        TestRunner.addResult(`tokenizer.write() returned ${result}, false expected`);

      for (var sample of [3, 15, 50]) {
        tokenizer = new BalancedJSONTokenizer(
            TestRunner.addResult.bind(TestRunner), true);
        TestRunner.addResult('\nRunning by ' + sample + ':');
        for (var i = 0; i < string.length; i += sample) {
          var result = tokenizer.write(string.substring(i, i + sample));
          var expectedResult = (i + sample < string.length);
          if (!!result !== expectedResult)
            TestRunner.addResult(`tokenizer.write() returned ${result}, ${expectedResult} expected`);
        }
      }
      next();
    },

    function testGarbageAfterObject(next) {
      var testString = '[{a: \'b\'}], {\'x\': {a: \'b\'}}';
      TestRunner.addResult('\nParsing ' + testString);
      var tokenizer = new BalancedJSONTokenizer(
          TestRunner.addResult.bind(TestRunner), true);
      var result = tokenizer.write(testString);
      TestRunner.addResult(`tokenizer.write() returned ${result}, false expected`);
      next();
    }
  ]);
})();
