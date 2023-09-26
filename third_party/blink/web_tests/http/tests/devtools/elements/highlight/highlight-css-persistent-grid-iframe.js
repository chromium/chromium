// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test verifies that persistent grid in iframe are positioned correctly.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <!DOCTYPE html>
    <iframe style="margin: 5em; width: 600px; height: 600px;" srcdoc="<style>div{width:500px;height:500px;display: grid;grid-gap: 10px;grid-template-columns: 120px  120px  120px;grid-template-areas:'....... header  header' 'sidebar content content'; background-color: #fff; color: #444;grid-template-rows:1fr 1fr;</style><div id='grid'>"></iframe>
  `);

  await new Promise(resolve => {
    ElementsTestRunner.dumpInspectorGridHighlightsJSON(['grid'], resolve);
  });

  TestRunner.completeTest();
})();
