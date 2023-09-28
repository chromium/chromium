// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Verifies Bindings.cssWorkspaceBinding.propertyUILocation functionality\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('resources/property-ui-location.html');
  SourcesTestRunner.waitForScriptSource('source-url.css', onUISourceCodeCreated);

  function onUISourceCodeCreated() {
    ElementsTestRunner.nodeWithId('inspected', onNodeFound);
  }

  function onNodeFound(node) {
    TestRunner.cssModel.getMatchedStyles(node.id).then(onMatchedStyles);
  }

  function onMatchedStyles(matchedResult) {
    var styles = matchedResult.nodeStyles();
    for (var style of styles) {
      if (style.type !== SDK.CSSStyleDeclaration.Type.Regular)
        continue;
      var properties = style.allProperties();
      for (var property of properties) {
        if (!property.range)
          continue;
        var uiLocation = Bindings.cssWorkspaceBinding.propertyUILocation(property, true);
        TestRunner.addResult(String.sprintf(
            '%s -> %s:%d:%d', property.name, uiLocation.uiSourceCode.name(), uiLocation.lineNumber,
            uiLocation.columnNumber));
        var uiLocation = Bindings.cssWorkspaceBinding.propertyUILocation(property, false);
        TestRunner.addResult(String.sprintf(
            '%s -> %s:%d:%d', property.value, uiLocation.uiSourceCode.name(), uiLocation.lineNumber,
            uiLocation.columnNumber));
      }
    }
    TestRunner.completeTest();
  }
})();
