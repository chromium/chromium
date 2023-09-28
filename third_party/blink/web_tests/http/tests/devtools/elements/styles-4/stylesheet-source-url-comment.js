// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests that stylesheets with sourceURL comment are shown in the Sources panel.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('resources/stylesheet-source-url-comment.html');

  function forEachHeaderMatchingURL(url, handler) {
    var headers = TestRunner.cssModel.styleSheetHeaders();
    for (var i = 0; i < headers.length; ++i) {
      if (headers[i].sourceURL.indexOf(url) !== -1)
        handler(headers[i]);
    }
  }

  function checkHeaderHasSourceURL(header) {
    TestRunner.assertTrue(header.hasSourceURL);
  }

  TestRunner.runTestSuite([
    function testSourceURLCommentInInlineScript(next) {
      SourcesTestRunner.showScriptSource('stylesheet-source-url-comment.html', didShowSource);

      function didShowSource(sourceFrame) {
        function checkHeaderDoesNotHaveSourceURL(header) {
          TestRunner.assertTrue(!header.hasSourceURL, 'hasSourceURL flag is set for inline stylesheet');
        }

        forEachHeaderMatchingURL('source-url-comment.html', checkHeaderDoesNotHaveSourceURL);
        next();
      }
    },

    function testSourceURLComment(next) {
      SourcesTestRunner.showScriptSource('css/addedInlineStylesheet.css', didShowSource);
      TestRunner.evaluateInPage('setTimeout(addInlineStyleSheet, 0)');

      function didShowSource(sourceFrame) {
        TestRunner.addResult(sourceFrame.textEditor.text());
        forEachHeaderMatchingURL('addedInlineStylesheet', checkHeaderHasSourceURL);
        next();
      }
    },

    function testDeprecatedSourceURLComment(next) {
      SourcesTestRunner.showScriptSource('css/addedInlineStylesheetDeprecated.css', didShowSource);
      TestRunner.evaluateInPage('setTimeout(addInlineStyleSheetDeprecated, 0)');

      function didShowSource(sourceFrame) {
        TestRunner.addResult(sourceFrame.textEditor.text());
        forEachHeaderMatchingURL('addedInlineStylesheetDeprecated', checkHeaderHasSourceURL);
        next();
      }
    },

    function testNonRelativeURL(next) {
      SourcesTestRunner.showScriptSource('css/nonRelativeInlineStylesheet.css', didShowSource);
      TestRunner.evaluateInPage('setTimeout(addInlineStyleSheetNonRelative, 0)');

      function didShowSource(sourceFrame) {
        TestRunner.addResult(sourceFrame.textEditor.text());
        forEachHeaderMatchingURL('nonRelativeInlineStyleSheet.css', checkHeaderHasSourceURL);
        next();
      }
    },

    function testMultiple(next) {
      SourcesTestRunner.showScriptSource('css/addedInlineStylesheetMultiple.css', didShowSource);
      TestRunner.evaluateInPage('setTimeout(addInlineStyleSheetMultiple, 0)');

      function didShowSource(sourceFrame) {
        TestRunner.addResult(sourceFrame.textEditor.text());
        forEachHeaderMatchingURL('addInlineStyleSheetMultiple.css', checkHeaderHasSourceURL);
        next();
      }
    }
  ]);
})();
