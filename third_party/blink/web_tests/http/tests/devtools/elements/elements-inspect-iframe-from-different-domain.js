// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Test that web inspector can select element in an iframe even if the element was created via createElement of document other than iframe's document. Bug 60031\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <iframe style="width:400px"></iframe>
    `);
  await TestRunner.evaluateInPagePromise(`
      var el1;
      var el2;
      function createDynamicElements()
      {
          var mainDoc = document;
          var frameDoc = window.frames[0].document;

          el1 = mainDoc.createElement('div');
          el1.id = "main-frame-div";
          el2 = frameDoc.createElement('div');
          el2.id = "iframe-div";

          el1.innerHTML = 'Element created via &lt;main document>.createElement';
          el2.innerHTML = 'Element created via &lt;frame document>.createElement';

          frameDoc.body.appendChild(el1);
          frameDoc.body.appendChild(el2);
      }
  `);

  TestRunner.evaluateInPage('createDynamicElements()', step1);

  function selectedNodeId() {
    var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
    if (!selectedElement)
      return '<no selected node>';
    return selectedElement.node().getAttribute('id');
  }

  function step1() {
    ConsoleTestRunner.evaluateInConsole('inspect(el1)', step2);
  }

  function step2() {
    TestRunner.deprecatedRunAfterPendingDispatches(step3);
  }

  function step3() {
    var id = selectedNodeId();
    if (id === 'main-frame-div')
      TestRunner.addResult('PASS: selected node  with id \'' + id + '\'');
    else
      TestRunner.addResult('FAIL: unexpected selection ' + id);
    // Frame was changed to the iframe. Moving back to the top frame.
    ConsoleTestRunner.evaluateInConsole('inspect(window.frameElement.parentElement)', step4);
  }

  function step4() {
    TestRunner.deprecatedRunAfterPendingDispatches(step5);
  }

  function step5() {
    ConsoleTestRunner.evaluateInConsole('inspect(el2)', step6);
  }

  function step6() {
    TestRunner.deprecatedRunAfterPendingDispatches(step7);
  }

  function step7() {
    var id = selectedNodeId();
    if (id === 'iframe-div')
      TestRunner.addResult('PASS: selected node  with id \'' + id + '\'');
    else
      TestRunner.addResult('FAIL: unexpected selection ' + id);
    TestRunner.completeTest();
  }
})();
