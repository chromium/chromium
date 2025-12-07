// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as TextUtils from 'devtools/models/text_utils/text_utils.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(
      `Tests that adding a new rule creates inspector stylesheet resource and allows its live editing.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected">Text</div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', onStylesSelected);

  function onStylesSelected(node) {
    Workspace.Workspace.WorkspaceImpl.instance().addEventListener(Workspace.Workspace.Events.WorkingCopyChanged, onWorkingCopyChanged);
    ElementsTestRunner.addNewRule('#inspected', new Function());
  }

  function onWorkingCopyChanged(event) {
    Workspace.Workspace.WorkspaceImpl.instance().removeEventListener(Workspace.Workspace.Events.WorkingCopyChanged, onWorkingCopyChanged);
    var uiSourceCode = event.data.uiSourceCode;
    TestRunner.addResult('Inspector stylesheet URL: ' + uiSourceCode.displayName());
    uiSourceCode.requestContentData().then(TextUtils.ContentData.ContentData.asDeferredContent).then(printContent(onContent));

    function onContent() {
      TestRunner.addResult('\nSetting new content');
      uiSourceCode.setWorkingCopy('#inspected { background-color: green; }');
      uiSourceCode.commitWorkingCopy();
      onUpdatedWorkingCopy(uiSourceCode);
    }
  }

  function onUpdatedWorkingCopy(uiSourceCode) {
    uiSourceCode.requestContentData().then(TextUtils.ContentData.ContentData.asDeferredContent).then(printContent(selectNode));
    function selectNode() {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', dumpStyles);
    }

    async function dumpStyles() {
      await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
      TestRunner.completeTest();
    }
  }

  function printContent(next) {
    function result({ content, error, isEncoded }) {
      TestRunner.addResult('Inspector stylesheet content:');
      TestRunner.addResult(content);
      if (next)
        next();
    }
    return result;
  }
})();
