// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Bindings from 'devtools/models/bindings/bindings.js';
import * as Platform from 'devtools/core/platform/platform.js';

(async function() {
  TestRunner.addResult(`Tests that styles sidebar pane does not leak any LiveLocations.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #first {
          color: blue;
      }

      #second {
          line-height: 1em;
          border: 1px solid black;
      }

      #third {
          margin: 1px 1px 0 0;
          padding: 10px;
          background-color: blue;
          border: 1px solid black;
      }
      </style>
      <div id="first">First element to select</div>
      <div id="second">Second element to select</div>
      <div id="third">Second element to select</div>
    `);

  var initialLiveLocationsCount;
  TestRunner.runTestSuite([
    function selectInitialNode(next) {
      ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('first', next);
    },

    function saveInitialLiveLocationsCount(next) {
      initialLiveLocationsCount = countLiveLocations();
      next();
    },

    function rebuildStylesSidebarPaneMultipleTimes(next) {
      var elementsToSelect = ['second', 'third', 'second', 'first', 'third', 'first'];
      function loopThroughElements() {
        if (!elementsToSelect.length) {
          next();
          return;
        }
        ElementsTestRunner.selectNodeAndWaitForStylesWithComputed(elementsToSelect.shift(), loopThroughElements);
      }
      loopThroughElements();
    },

    function compareLiveLocationsCount(next) {
      var liveLocationsCount = countLiveLocations();
      if (liveLocationsCount !== initialLiveLocationsCount)
        TestRunner.addResult(Platform.StringUtilities.sprintf(
            'ERROR: LiveLocations count is growing! Expected: %d found: %d', initialLiveLocationsCount,
            liveLocationsCount));
      else
        TestRunner.addResult('SUCCESS: LiveLocations count do not grow.');
      next();
    }
  ]);

  function countLiveLocations() {
    var locationsCount = 0;
    var modelInfos = Bindings.CSSWorkspaceBinding.CSSWorkspaceBinding.instance().modelToInfo.values();
    for (var modelInfo of modelInfos)
      locationsCount += modelInfo.locations.valuesArray().length;
    return locationsCount;
  }
})();
