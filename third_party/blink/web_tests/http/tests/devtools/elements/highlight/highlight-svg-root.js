// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `This test verifies the position and size of the highlight rectangles overlayed on an SVG root element.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE html>
      <style>

      body {
          margin: 0;
      }
      #svg-root {
          margin: 5px;
          border: solid 10px aqua;
          padding: 15px;
          background-color: blue;
      }

      </style>
      <svg id="svg-root" width="100" height="100" viewBox="0 0 50 50"/>
      <p id="description"></p>
    `);

  ElementsTestRunner.dumpInspectorHighlightJSON('svg-root', TestRunner.completeTest.bind(TestRunner));
})();
