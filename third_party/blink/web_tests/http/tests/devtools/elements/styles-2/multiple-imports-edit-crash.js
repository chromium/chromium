// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that modifying stylesheet text with multiple @import at-rules does not crash.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <head>
      <style>
        @import url(../styles/resources/multiple-imports-edit-crash-1.css);
        @import url(../styles/resources/multiple-imports-edit-crash-2.css);
        @import url(../styles/resources/multiple-imports-edit-crash-1.css);
        #inspected {
            color: green;
        }
      </style>
    </head>
    <body>
      <div id="inspected">Text</div>
    </body>
  `);
  var initialAddsExpected = 4;
  var initialAdded = [];
  await new Promise(f => TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetAdded, function styleSheetAdded(event) {
    const name = resourceName(event.data.sourceURL);
    if (name) {
      // Don't include the <style> element sheet.
      initialAdded.push(name);
    }
    if (!(--initialAddsExpected)) {
      initialAdded.sort();
      TestRunner.addResult('Initially added:');
      TestRunner.addResult(initialAdded.join('\n'));
      f();
    }
  }, this));

  TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetAdded, styleSheetAdded, this);
  TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetRemoved, styleSheetRemoved, this);
  ElementsTestRunner.nodeWithId('inspected', nodeFound);

  function nodeFound(node) {
    TestRunner.cssModel.getMatchedStyles(node.id).then(matchedStylesCallback);
  }

  var styleSheetId;

  function matchedStylesCallback(matchedResult) {
    styleSheetId = matchedResult.nodeStyles()[1].styleSheetId;
    TestRunner.addResult('Setting stylesheet text...');
    TestRunner.CSSAgent.setStyleSheetText(
        styleSheetId,
        '@import url(../styles/resources/multiple-imports-edit-crash-1.css);\n@import url(../styles/resources/multiple-imports-edit-crash-2.css);\n#inspected { color: black }\n');
  }

  var addsExpected = 2;
  var removesExpected = 3;
  var added = [];
  var removed = [];

  function styleSheetAdded(event) {
    added.push(resourceName(event.data.sourceURL));

    if (!(--addsExpected)) {
      added.sort();
      TestRunner.addResult('Added:');
      TestRunner.addResult(added.join('\n'));
      TestRunner.completeTest();
    }
  }

  function styleSheetRemoved(event) {
    removed.push(resourceName(event.data.sourceURL));

    if (!(--removesExpected)) {
      removed.sort();
      TestRunner.addResult('Removed:');
      TestRunner.addResult(removed.join('\n'));
    }
  }

  function resourceName(url) {
    return url.substring(url.lastIndexOf('/') + 1);
  }
})();
