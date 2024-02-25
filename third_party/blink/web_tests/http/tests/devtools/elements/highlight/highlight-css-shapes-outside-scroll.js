// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Highlight CSS shapes outside scroll.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE html>
      <style>

      .float {
          width: 8em;
          height: 8em;
          float: left;
          shape-margin: 2em;
          margin: 1em;
      }

      .circle {
          background-color:blue;
          shape-outside: circle(closest-side at center);
          -webkit-clip-path: circle(closest-side at center);
      }

      </style>
      <div class="float circle" id="circle"> </div>
    `);

  var i = 0;
  function dump() {
    ElementsTestRunner.dumpInspectorHighlightJSON('circle', i == 0 ? scroll : TestRunner.completeTest.bind(TestRunner));
  }
  function scroll() {
    window.scrollTo(0, 100);
    ++i;
    dump();
  }
  dump();
})();
