// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as BindingsModule from 'devtools/models/bindings/bindings.js';

(async function() {
  TestRunner.addResult(
      `Verify that persistence does not overwrite CSS files when CSS model reports error on getStyleSheetText.\n`);
  await TestRunner.loadHTML(`
      <style>
      body {
          color: red;
      }
      /*# sourceURL=http://127.0.0.1:8000/simple.css */
      </style>
    `);

  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fsUISourceCode, fs;

  TestRunner.runTestSuite([
    function initializeTestFileSystem(next) {
      TestRunner.waitForUISourceCode('simple.css')
          .then(uiSourceCode => uiSourceCode.requestContent())
          .then(onCSSContent);

      function onCSSContent({ content, error, isEncoded }) {
        fs = new BindingsTestRunner.TestFileSystem('/var/www');
        BindingsTestRunner.addFiles(fs, {
          'simple.css': {content: content},
        });
        fs.reportCreated(next);
      }
    },

    function waitForPersistenceBinding(next) {
      testMapping.addBinding('simple.css');
      BindingsTestRunner.waitForBinding('simple.css').then(onBinding);

      function onBinding(binding) {
        fsUISourceCode = binding.fileSystem;
        fsUISourceCode.requestContent().then(onContent);
      }

      function onContent({ content, error, isEncoded }) {
        TestRunner.addResult('Initial content of file:///var/www/simple.css');
        TestRunner.addResult('----\n' + content + '\n----');
        next();
      }
    },

    function breakCSSModelProtocol(next) {
      // Nullify console.error since it dumps style sheet Ids and make test flake.
      console.error = function() {};

      var styleSheet =
          TestRunner.cssModel.styleSheetHeaders().find(header => header.contentURL().endsWith('simple.css'));
      // Make CSSModel constantly return errors on all getStyleSheetText requests.
      TestRunner.override(TestRunner.cssModel.agent, 'invoke_getStyleSheetText', throwProtocolError, true);
      // Set a new stylesheet text
      TestRunner.cssModel.setStyleSheetText(styleSheet.id, 'body {color: blue}');
      // Expect StylesSourceMapping to sync styleSheet with network UISourceCode.
      // Persistence acts synchronously.
      TestRunner.addSniffer(BindingsModule.StylesSourceMapping.StyleFile.prototype, 'styleFileSyncedForTest', next);

      function throwProtocolError(styleSheetId) {
        TestRunner.addResult('Protocol Error: FAKE PROTOCOL ERROR');
        return Promise.resolve({ getError: () => 'FAKE PROTOCOL ERROR'});
      }
    },

    function onStylesSourcemappingSynced(next) {
      TestRunner.addResult('Updated content of file:///var/www/simple.css');
      TestRunner.addResult('----\n' + fsUISourceCode.content() + '\n----');
      next();
    }
  ]);
})();
