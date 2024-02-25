// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that colors are not re-formatted inside url(...) when editing property values.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected1" style="background: white">&quot;white&quot; background</div>
      <div id="inspected2" style="background: url( white )">&quot;url( white )&quot; background</div>
      <div id="inspected3" style="background: url(white.png)">&quot;url(white.png)&quot; background</div>
      <div id="inspected4" style="background: url(../foo/white.png)">&quot;url(../foo/white.png)&quot; background</div>
      <div id="inspected5" style="background: green url(white)">&quot;green url(white)&quot; background</div>
      <div id="inspected6" style="background: url(white) green">&quot;url(white) green&quot; background</div>
      <div id="inspected7" style="background: url(white) green, url(green)">&quot;url(white) green, url(green)&quot; background</div>
      <div id="inspected8" style="background: url(white), url(green)">&quot;url(white), url(green)&quot; background</div>
      <div id="inspected9" style="background: hsl(0, 50%, 50%) url(white)">&quot;hsl(0, 50%, 50%) url(white)&quot; background</div>
      <div id="inspected10" style="background: url(white) hsl(0, 50%, 50%)">&quot;url(white) hsl(0, 50%, 50%)&quot; background</div>
      <div id="inspected11" style="background: url(../black/white.png)">&quot;url(../black/white.png)&quot; background</div>
    `);

  var maxIndex = 11;
  var idIndex = 1;

  selectDivAndEditValue();

  function selectDivAndEditValue() {
    ElementsTestRunner.selectNodeAndWaitForStyles('inspected' + idIndex++, editCallback);
  }

  function editCallback() {
    var treeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('background');
    treeElement.startEditingValue();
    TestRunner.addResult(treeElement.valueElement.textContent);
    if (idIndex <= maxIndex)
      selectDivAndEditValue();
    else
      TestRunner.completeTest();
  }
})();
