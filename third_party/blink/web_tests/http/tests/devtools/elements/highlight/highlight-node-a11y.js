// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
    TestRunner.addResult(
        `This test verifies a11y attributes for a node.\n`);
    await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
    await TestRunner.showPanel('elements');
    await TestRunner.loadHTML(`
        <!DOCTYPE html>
        <button id="button">click</button>
        <input id="input"></input>
        <div id="div" tabindex="0" role="button" aria-label="click">Save</div>
      `);

    function dumpHighlight(id) {
        return new Promise(resolve => ElementsTestRunner.dumpInspectorHighlightJSON(id, resolve));
    }

    await dumpHighlight('button');
    await dumpHighlight('input');
    await dumpHighlight('div');

    TestRunner.completeTest();
})();