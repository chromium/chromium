// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as TextUtils from 'devtools/models/text_utils/text_utils.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Verifies that CSSStyleSheetHeader.originalContentProvider() indeed returns original content.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
<style>
div { color: red; }
/*# sourceURL=set-style.css */
</style>
<style>
div {}
/*# sourceURL=set-selector.css */
</style>
<style>
@media (all) { }
/*# sourceURL=set-media.css */
</style>
<style>
@keyframes animation { 100% { color: red; } }
/*# sourceURL=set-keyframe-key.css */
</style>
<style>
div {}
/*# sourceURL=add-rule.css */
</style>
<style>
div {}
/*# sourceURL=set-text.css */
</style>
<div id="inspected"></div>
    `);
  await ElementsTestRunner.selectNodeAndWaitForStylesPromise('inspected');

  TestRunner.addSniffer(SDK.CSSModel.CSSModel.prototype, 'originalContentRequestedForTest', onOriginalContentRequested, true);
  function onOriginalContentRequested(header) {
    TestRunner.addResult('original content loaded for header: ' + header.sourceURL);
  }

  var headers = TestRunner.cssModel.styleSheetHeaders();
  TestRunner.runTestSuite([
    function testSetStyle(next) {
      var header = headers.find(header => header.sourceURL.endsWith('set-style.css'));
      TestRunner.cssModel.setStyleText(header.id, new TextUtils.TextRange.TextRange(1, 5, 1, 18), 'EDITED: EDITED', true)
          .then(success => onEdit(header, success))
          .then(next);
    },

    function testSetSelector(next) {
      var header = headers.find(header => header.sourceURL.endsWith('set-selector.css'));
      TestRunner.cssModel.setSelectorText(header.id, new TextUtils.TextRange.TextRange(1, 0, 1, 3), 'EDITED')
          .then(success => onEdit(header, success))
          .then(next);
    },

    function testSetMedia(next) {
      var header = headers.find(header => header.sourceURL.endsWith('set-media.css'));
      TestRunner.cssModel.setMediaText(header.id, new TextUtils.TextRange.TextRange(1, 7, 1, 12), 'EDITED')
          .then(success => onEdit(header, success))
          .then(next);
    },

    function testSetKeyframeKey(next) {
      var header = headers.find(header => header.sourceURL.endsWith('set-keyframe-key.css'));
      TestRunner.cssModel.setKeyframeKey(header.id, new TextUtils.TextRange.TextRange(1, 23, 1, 27), 'from')
          .then(success => onEdit(header, success))
          .then(next);
    },

    function testAddRule(next) {
      var header = headers.find(header => header.sourceURL.endsWith('add-rule.css'));
      TestRunner.cssModel.addRule(header.id, 'EDITED {}\n', new TextUtils.TextRange.TextRange(1, 0, 1, 0))
          .then(success => onEdit(header, success))
          .then(next);
    },

    function testSetStyleSheetText(next) {
      var header = headers.find(header => header.sourceURL.endsWith('set-text.css'));
      TestRunner.cssModel.setStyleSheetText(header.id, 'EDITED {}', true)
          .then(success => onEdit(header, success))
          .then(next);
    },
  ]);

  function onEdit(header, success) {
    if (success !== null && !success) {
      TestRunner.addResult('Failed to run edit operation.');
      TestRunner.completeTest();
      return;
    }
    var contents = [
      header.originalContentProvider().requestContent(),
      header.requestContent(),
    ];
    return Promise.all(contents).then(onContents);
  }

  function onContents(contents) {
    TestRunner.addResult('== Original ==');
    TestRunner.addResult(contents[0].content.trim());
    TestRunner.addResult('== Current ==');
    TestRunner.addResult(contents[1].content.trim());
  }
})();
