// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Highlight CSS shapes.\n`);
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

      .contentBox {
          background-color:blue;
          border-radius: 2em 4em 2em 2em;
          border-width: 3em 1em 2em 1em;
          padding: 1em 1em 1em 2em;
          margin: 2em 1em 1em 1em;
          shape-outside: content-box;
          -webkit-clip-path: content-box;
      }

      .paddingBox {
          background-color: blue;
          border-radius: 2em 4em 2em 2em;
          border-width: 3em 1em 2em 1em;
          padding: 1em 1em 1em 2em;
          margin: 2em 1em 1em 1em;
          shape-outside: padding-box;
          clip-path: padding-box;
      }

      .insetSimpleRound {
          background-color:green;
          shape-outside: inset(30% round 20%);
          -webkit-clip-path: inset(30% round 20%);
      }

      .insetComplexRound {
          background-color:blue;
          shape-outside: inset(10% round 10% 40% 10% 40%);
          -webkit-clip-path: inset(10% round 10% 40% 10% 40%);
      }

      .ellipse {
          background-color:green;
          height: 5em;
          shape-outside: ellipse(closest-side closest-side);
          -webkit-clip-path: ellipse(closest-side closest-side);
      }

      .raster {
          background-color: blue;
          shape-outside: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='100px' height='100px'><rect width='100' height='100' fill='green'/></svg>");
      }

      .polygon {
          background-color: green;
          shape-outside: polygon(0px 0px, 0px 200px, 200px 0px);
      }

      </style>
      <div class="float circle" id="circle"> </div>
      <div class="float insetSimpleRound" id="insetSimpleRound"> </div>
      <div class="float insetComplexRound" id="insetComplexRound"> </div>
      <div class="float ellipse" id="ellipse"> </div>
      <div class="float contentBox" id="contentBox"> </div>
      <div class="float polygon" id="polygon"> </div>
      <div class="float raster" id="raster"> </div>
      <div style="writing-mode:sideways-lr;">
        <div class="float contentBox" id="slrContentBox"></div>
        <div class="float paddingBox" id="slrPaddingBox"></div>
      </div>
      <div style="writing-mode:sideways-rl;">
        <div class="float contentBox" id="srlContentBox"></div>
        <div class="float paddingBox" id="srlPaddingBox"></div>
      </div>
    `);

  var list = ['circle', 'insetSimpleRound', 'insetComplexRound', 'ellipse',
              'contentBox', 'polygon', 'raster', 'slrContentBox',
              'slrPaddingBox', 'srlContentBox', 'srlPaddingBox'];
  var index = 0;
  function nextNode() {
    var nodeId = String(list[index++]);
    ElementsTestRunner.dumpInspectorHighlightJSON(
        nodeId, (index == list.length) ? TestRunner.completeTest.bind(TestRunner) : nextNode);
  }
  nextNode();
})();
