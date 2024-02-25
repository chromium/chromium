// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Verifies that SelectUISourceCodeDialog rendering works properly.\n`);
  await TestRunner.evaluateInPagePromise(`    function dummy1() { }
      //# sourceURL=http://test/helloWorld12.js
    `);
  await TestRunner.evaluateInPagePromise(`    function dummy2() { }
      //# sourceURL=http://test/some/very-long-url/which/usually/breaks-rendering/due-to/trancation/so/that/the-path-is-cut-appropriately/and-no-horizontal-scrollbars/are-shown.js
    `);

  var provider = new SourcesModule.FilteredUISourceCodeListProvider.FilteredUISourceCodeListProvider();
  provider.attach();

  TestRunner.runTestSuite([
    function testRenderingInNameOnly(next) {
      checkQuery('12');
      next();
    },

    function testRenderingInPathAndName(next) {
      checkQuery('te12');
      next();
    },

    function testRenderingInNameInTruncatedPath(next) {
      checkQuery('shown.js');
      next();
    },

    function testRenderingInTruncatedPath(next) {
      checkQuery('usually-shown.js');
      next();
    },
  ]);

  function checkQuery(query) {
    TestRunner.addResult('filter query: ' + query);
    var titleElement = document.createElement('div');
    var subtitleElement = document.createElement('div');
    var outputs = [];
    for (var i = 0; i < provider.itemCount(); ++i) {
      provider.renderItem(i, query, titleElement, subtitleElement);
      if (!subtitleElement.textContent.startsWith('test'))
        continue;
      var text = elementHighlightedText(titleElement);
      text += '\n' + elementHighlightedText(subtitleElement);
      outputs.push(text);
    }
    outputs.sort();
    TestRunner.addResult(outputs.join('\n--------------------\n'));
  }

  function elementHighlightedText(element) {
    var text = '';
    for (var i = 0; i < element.childNodes.length; ++i) {
      var node = element.childNodes[i];
      if (node.nodeType === Node.TEXT_NODE)
        text += node.textContent;
      else
        text += Platform.StringUtilities.sprintf('[%s]', node.textContent);
    }
    return text;
  }
})();
