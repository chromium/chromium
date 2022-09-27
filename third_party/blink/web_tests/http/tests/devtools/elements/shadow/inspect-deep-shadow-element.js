// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that inspect element action works for deep shadow elements.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div>
        <div>
          <div id="host"></div>
          <span id="hostOpen"></span>
        </div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
          document.querySelector('#host').attachShadow({mode: 'open'}).innerHTML = "<div><div><span id='shadow'>Shadow</span></div></div>";
          document.querySelector('#hostOpen').attachShadow({ mode: "open" }).innerHTML = "<div><div><span id='shadow-open'>Shadow</span></div></div>";
    `);

  ElementsTestRunner.firstElementsTreeOutline().addEventListener(
      Elements.ElementsTreeOutline.Events.SelectedNodeChanged, selectedNodeChanged);

  var tests = [
    ['shadow', 'inspect(host.shadowRoot.firstChild.firstChild.firstChild)'],
    ['shadow-open', 'inspect(hostOpen.shadowRoot.firstChild.firstChild.firstChild)']
  ];

  function selectedNodeChanged(event) {
    var node = event.data.node;
    if (!node)
      return;
    if (node.getAttribute('id') == tests[0][0]) {
      TestRunner.addResult(Elements.DOMPath.xPath(node, false));
      TestRunner.addResult(Elements.DOMPath.jsPath(node, false));
      tests.shift();
      nextTest();
    }
  }

  function nextTest() {
    if (!tests.length) {
      TestRunner.completeTest();
      return;
    }
    ConsoleTestRunner.evaluateInConsole(tests[0][1]);
  }

  nextTest();
})();
