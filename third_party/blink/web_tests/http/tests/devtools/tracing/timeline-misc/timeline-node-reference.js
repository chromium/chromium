// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a Layout event\n`);
  await TestRunner.loadLegacyModule('elements');
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.loadLegacyModule('components');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <style>
      .relayout-boundary {
          overflow: hidden;
          width: 100px;
          height: 100px;
          position: relative;
      }
      </style>
      <div>
          <div id="boundary1" class="relayout-boundary">
              <div>
                  <div id="invalidate1"><div>text</div></div>
              </div>
          </div>
      </div>
      <div id="boundary2" class="relayout-boundary">
          <div>
              <div id="invalidate2"><div>text</div></div>
          </div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var element = document.getElementById("invalidate1");
          element.style.marginTop = "10px";
          element = document.getElementById("invalidate2");
          element.style.marginTop = "15px";
          var unused = element.offsetHeight;
      }
  `);

  var rows;

  TestRunner.evaluateInPage('var unused = document.body.offsetWidth;', async function() {
    const records = await PerformanceTestRunner.evaluateWithTimeline('performActions()');
    const layoutEvent = PerformanceTestRunner.findTimelineEvent(TimelineModel.TimelineModel.RecordType.Layout);
    UI.context.addFlavorChangeListener(SDK.DOMNode, onSelectedNodeChanged);
    var model = UI.panels.timeline.performanceModel.timelineModel();
    var element = await Timeline.TimelineUIUtils.buildTraceEventDetails(layoutEvent, model, new Components.Linkifier(), true);
    rows = Array.from(element.querySelectorAll('.timeline-details-view-row'));
    clickNextLayoutRoot();
  });

  async function clickNextLayoutRoot() {
    while (rows.length) {
      let row = rows.shift();
      if (row.firstChild.textContent.indexOf('Layout root') !== -1) {
        row.lastChild.firstChild.shadowRoot.lastChild.click();
        return;
      }
    }
    UI.context.removeFlavorChangeListener(SDK.DOMNode, onSelectedNodeChanged);
    TestRunner.completeTest();
  }

  function onSelectedNodeChanged() {
    var node = UI.panels.elements.selectedDOMNode();
    // We may first get an old selected node while switching to the Elements panel.
    if (node.nodeName() === 'BODY')
      return;
    TestRunner.addResult('Layout root node id: ' + node.getAttribute('id'));
    clickNextLayoutRoot();
  }
})();
