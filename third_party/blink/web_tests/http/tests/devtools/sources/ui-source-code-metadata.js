// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Verify that network UISourceCode has metadata.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.addStylesheetTag('resources/style.css');

  TestRunner.runTestSuite([
    function testCachedResource(next) {
      SourcesTestRunner.waitForScriptSource('style.css', onStyleSheet);

      function onStyleSheet(uiSourceCode) {
        dumpMetadata(uiSourceCode, next);
      }
    },

    async function testDynamicResource(next) {
      await TestRunner.addScriptTag('resources/script.js');
      SourcesTestRunner.waitForScriptSource('script.js', onScript);

      function onScript(uiSourceCode) {
        dumpMetadata(uiSourceCode, next);
      }
    },

    function testInlinedSourceMapSource(next) {
      SourcesTestRunner.waitForScriptSource('style.scss', onSourceMapSource);

      function onSourceMapSource(uiSourceCode) {
        dumpMetadata(uiSourceCode, next);
      }
    },
  ]);

  function dumpMetadata(uiSourceCode, next) {
    uiSourceCode.requestMetadata().then(onMetadata);

    function onMetadata(metadata) {
      TestRunner.addResult('Metadata for UISourceCode: ' + uiSourceCode.url());
      if (!metadata) {
        TestRunner.addResult('    Metadata is EMPTY');
        next();
        return;
      }
      var contentSize = metadata.contentSize;
      var time = metadata.modificationTime ? '<Date>' : 'null';
      TestRunner.addResult('    content size: ' + contentSize);
      TestRunner.addResult('    modification time: ' + time);
      next();
    }
  }
})();
