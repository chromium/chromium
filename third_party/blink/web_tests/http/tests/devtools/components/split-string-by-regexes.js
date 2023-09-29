// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as TextUtils from 'devtools/models/text_utils/text_utils.js';

(async function() {
  TestRunner.addResult(`Tests TextUtils.TextUtils.Utils.splitStringByRegexes.\n`);


  TestRunner.runTestSuite([
    function testSimple(next) {
      var regexes = [/hello/g, /[0-9]+/g];
      var results = TextUtils.TextUtils.Utils.splitStringByRegexes('hello123hello123', regexes);
      dumpResults(results);
      next();
    },

    function testMatchAtStart(next) {
      var regexes = [/yes/g];
      var results = TextUtils.TextUtils.Utils.splitStringByRegexes('yes thank you', regexes);
      dumpResults(results);
      next();
    },

    function testMatchAtEnd(next) {
      var regexes = [/you/g];
      var results = TextUtils.TextUtils.Utils.splitStringByRegexes('yes thank you', regexes);
      dumpResults(results);
      next();
    },

    function testAvoidInnerMatch(next) {
      var regexes = [/url\("red\.com"\)/g, /red/g];
      var results = TextUtils.TextUtils.Utils.splitStringByRegexes('image: url("red.com")', regexes);
      dumpResults(results);
      next();
    },

    function testNoMatch(next) {
      var regexes = [/something/g];
      var results = TextUtils.TextUtils.Utils.splitStringByRegexes('nothing', regexes);
      dumpResults(results);
      next();
    },

    function testNoMatches(next) {
      var regexes = [/something/g, /123/g, /abc/g];
      var results = TextUtils.TextUtils.Utils.splitStringByRegexes('nothing', regexes);
      dumpResults(results);
      next();
    },

    function testComplex(next) {
      var regexes = [/\(([^)]+)\)/g, /okay/g, /ka/g];
      var results =
          TextUtils.TextUtils.Utils.splitStringByRegexes('Start. (okay) kit-kat okay (kale) ka( ) okay. End', regexes);
      dumpResults(results);
      next();
    }
  ]);

  function dumpResults(results) {
    for (var i = 0; i < results.length; i++) {
      var result = results[i];
      TestRunner.addResult('"' + result.value + '", ' + result.position + ', ' + result.regexIndex);
    }
  }
})();
