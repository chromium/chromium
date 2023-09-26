// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>

      body {
          margin: 0;
      }

      #container {
          position: absolute;
          overflow: hidden;
          left: 100px;
          top: 200px;
      }

      iframe {
          width: 200px;
          height: 200px;
          border: none;
      }

      </style>
      <div id="container">    <iframe id="svg-iframe" src="resources/highlight-svg-content-iframe.html"></iframe>
      </div>
    `);

  ElementsTestRunner.dumpInspectorHighlightJSON('svg-rect', TestRunner.completeTest.bind(TestRunner));
})();
