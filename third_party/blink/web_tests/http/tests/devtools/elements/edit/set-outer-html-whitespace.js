// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that DOMAgent.setOuterHTML can handle whitespace-only text nodes.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container" style="display:none">
        <child id="identity"></child>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      document.getElementById("identity").wrapperIdentity = "identity";
  `);

  async function setChildTextContent(textContent) {
    var text = ElementsTestRunner.containerText.replace(/<child id="identity">.*<\/child>/, `<child id="identity">${textContent}</child>`);
    TestRunner.addResult(`Setting textContent to "${textContent}"`)
    await TestRunner.DOMAgent.setOuterHTML(ElementsTestRunner.containerId, text);
    dumpEvents();
  }

  function dumpEvents() {
    ElementsTestRunner.events.sort();

    for (let i = 0; i < ElementsTestRunner.events.length; ++i)
      TestRunner.addResult(ElementsTestRunner.events[i]);

    ElementsTestRunner.events.length = 0; // 'events' is readonly.
    TestRunner.addResult("");
  }

  await new Promise(x => ElementsTestRunner.setUpTestSuite(x));
  await setChildTextContent(' ')
  await setChildTextContent('NOT_WHITESPACE')
  await setChildTextContent('OTHER_NOT_WHITESPACE')
  await setChildTextContent('   ')
  await setChildTextContent('')
  TestRunner.completeTest();
})();
