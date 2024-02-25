// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Changes from 'devtools/panels/changes/changes.js';
import * as Common from 'devtools/core/common/common.js';
import * as Bindings from 'devtools/models/bindings/bindings.js';
import * as TextUtils from 'devtools/models/text_utils/text_utils.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as WorkspaceDiff from 'devtools/models/workspace_diff/workspace_diff.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Tests that the changes sidebar contains the changed uisourcecodes.\n`);

  var fulfill = function() {};
  var workspace = new Workspace.Workspace.WorkspaceImpl();
  var project =
      new Bindings.ContentProviderBasedProject.ContentProviderBasedProject(workspace, 'mockProject', Workspace.Workspace.projectTypes.Network, '', false);
  var workspaceDiff = new WorkspaceDiff.WorkspaceDiff.WorkspaceDiffImpl(workspace);
  TestRunner.addSniffer(
      WorkspaceDiff.WorkspaceDiff.WorkspaceDiffImpl.prototype, 'uiSourceCodeProcessedForTest', modifiedStatusChanged, true);

  var uiSourceCodeList = new Changes.ChangesSidebar.ChangesSidebar(workspaceDiff);

  var firstUISC = addUISourceCode('first.css', '.first {color: red}');
  var secondUISC = addUISourceCode('second.css', '.second {color: red}');
  var thirdUISC = addUISourceCode('third.css', '.third {color: red}');
  uiSourceCodeList.show(UI.InspectorView.InspectorView.instance().element);

  TestRunner.runTestSuite([
    function initialState(next) {
      dumpAfterLoadingFinished().then(next);
    },
    function workingCopyChanged(next) {
      firstUISC.setWorkingCopy('.first {color: blue}');
      dumpAfterLoadingFinished().then(next);
    },
    function workingCopyComitted(next) {
      firstUISC.commitWorkingCopy();
      secondUISC.addRevision('.second {color: blue}');
      dumpAfterLoadingFinished().then(next);
    },
    function resetAll(next) {
      firstUISC.addRevision('.first {color: red}');
      secondUISC.addRevision('.second {color: red}');
      thirdUISC.addRevision('.third {color: red}');
      dumpAfterLoadingFinished().then(next);
    }

  ]);

  function modifiedStatusChanged() {
    if (!workspaceDiff.loadingUISourceCodes.size)
      fulfill();
  }

  function dumpUISourceCodeList() {
    uiSourceCodeList.treeoutline.rootElement().children().forEach(treeElement => {
      TestRunner.addResult(treeElement.title);
    });
  }

  function dumpAfterLoadingFinished() {
    var promise = new Promise(x => fulfill = x);
    modifiedStatusChanged();
    return promise.then(dumpUISourceCodeList);
  }

  function addUISourceCode(url, content) {
    return project.addContentProvider(
        url, TextUtils.StaticContentProvider.StaticContentProvider.fromString(url, Common.ResourceType.resourceTypes.Stylesheet, content));
  }
})();
