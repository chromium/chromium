// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as FormatterModule from 'devtools/models/formatter/formatter.js';

(async function() {
  TestRunner.addResult(`The test verifies the CSS outline functionality.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
<style id="styler">
@import url("some-url-to-load-css.css") print;
@charset "ISO-8859-15";
@namespace svg url(http://www.w3.org/2000/svg);
@font-face {
    font-family: "Example Font";
    src: url("/fonts/example");
}

@page {
    margin: 1in 1.5in;
}
@page :right {
    margin-right: 5cm; /* right pages only */
}
@page :first {
    margin-top: 8cm; /* extra top margin on the first page */
}

div { color: red }
#fluffy {
    border: 1px solid black;
    z-index: 1;
    /* -webkit-disabled-property: rgb(1, 2, 3) */
}
input:-moz-placeholder { text-overflow: ellipsis; }
.class-name, p /* style all paragraphs as well */ {
    border-color: blue;
    -lol-cats: "dogs" /* unexisting property */
}

@keyframes identifier {
    0% { top: 0; left: 0; }
    30% { top: 50px; }
    68%, 72% { left: 50px; }
    100% { top: 100px; left: 100%; }
}

svg|a {
    text-decoration: underline;
}

@media (max-width:500px) {
    span {
/*      font-family: Times New Roman; */
        -webkit-border-radius: 10px;
        font-family: "Example Font"
    }
}
</style>
    `);
  await TestRunner.evaluateInPagePromise(`
      function getCSS()
      {
          return document.querySelector("#styler").textContent;
      }
  `);

  function onRulesParsed(isLastChunk, rules) {
    for (var i = 0; i < rules.length; ++i)
      TestRunner.addObject(rules[i]);
    if (isLastChunk)
      TestRunner.completeTest();
  }

  function onStyleFetched(result) {
    FormatterModule.FormatterWorkerPool.formatterWorkerPool().parseCSS(result, onRulesParsed);
  }

  TestRunner.evaluateInPage('getCSS()', onStyleFetched);
})();
