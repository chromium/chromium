// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `This test verifies the position and size of the highlight rectangles overlayed on an SVG root element when the page is zoomed.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE html>
      <style>

      body {
          margin: 0;
      }
      #svg-root {
          position: relative;
          left: 10px;
          top: 20px;
          margin: 30px;
          border: solid 40px aqua;
          padding: 50px;
          background-color: blue;
      }

      </style>
      <svg id="svg-root" width="100" height="200" viewBox="0 0 50 100"/>
      <p id="description"></p>
      <!--

      Expected value calculations for #svg-root's highlight rectangles at 120% zoom:

      margin rect:
          left:   (10) * 1.2 == 12
          top:    (20) * 1.2 == 24
          width:  (100 + 2 * (30 + 40 + 50)) * 1.2 == 340 * 1.2 == 408
          height: (200 + 2 * (30 + 40 + 50)) * 1.2 == 440 * 1.2 == 528

      border rect:
          left:   (10 + 30) * 1.2 == 40 * 1.2 == 48
          top:    (20 + 30) * 1.2 == 50 * 1.2 == 60
          width:  (100 + 2 * (40 + 50)) * 1.2 == 280 * 1.2 == 336
          height: (200 + 2 * (40 + 50)) * 1.2 == 380 * 1.2 == 456

      padding rect:
          left:   (10 + 30 + 40) * 1.2 == 80 * 1.2 == 96
          top:    (20 + 30 + 40) * 1.2 == 90 * 1.2 == 108
          width:  (100 + 2 * (50)) * 1.2 == 200 * 1.2 == 240
          height: (200 + 2 * (50)) * 1.2 == 300 * 1.2 == 360

      content rect:
          left:   (10 + 30 + 40 + 50) * 1.2 == 130 * 1.2 == 156
          top:    (20 + 30 + 40 + 50) * 1.2 == 140 * 1.2 == 168
          width:  (100) * 1.2 == 120
          height: (200) * 1.2 == 240

      -->
      <div id="console"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      if (window.testRunner)
          testRunner.zoomPageIn();
  `);

  ElementsTestRunner.dumpInspectorHighlightJSON('svg-root', TestRunner.completeTest.bind(TestRunner));
})();
