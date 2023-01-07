// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that devtools can inspect text element under shadow root.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div><div><div id="host"></div></div></div>
    `);
  await TestRunner.evaluateInPagePromise(`
        var host = document.querySelector('#host');
        var sr = host.attachShadow({mode: 'open'});
        sr.innerHTML = "Text Text Text<br>Text Text Text";
    `);
  await TestRunner.evaluateInPagePromise(`
      function click()
      {
          var target = document.getElementById("host");
          var rect = target.getBoundingClientRect();
          // Simulate the mouse click over the target to trigger an event dispatch.
          if (window.eventSender) {
              eventSender.mouseMoveTo(rect.left + 10, rect.top + 10);
              eventSender.mouseDown();
              eventSender.mouseUp();
          }
      }
  `);

  TestRunner.overlayModel.setInspectMode(Protocol.Overlay.InspectMode.SearchForNode).then(step2);

  function step2() {
    ElementsTestRunner.firstElementsTreeOutline().addEventListener(
        Elements.ElementsTreeOutline.Events.SelectedNodeChanged, step3);
    TestRunner.evaluateInPage('click()');
  }

  function step3() {
    ElementsTestRunner.firstElementsTreeOutline().removeEventListener(
        Elements.ElementsTreeOutline.Events.SelectedNodeChanged, step3);
    var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
    TestRunner.addResult('Node selected: ' + selectedElement.node().getAttribute('id'));
    TestRunner.completeTest();
  }
})();
