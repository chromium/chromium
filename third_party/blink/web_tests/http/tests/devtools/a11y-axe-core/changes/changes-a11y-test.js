// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';

import * as Diff from "devtools/third_party/diff/diff.js";
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult('Tests accessibility in the Changes drawer.');

  const diff = [
    {0: Diff.Diff.Operation.Insert, 1: ['line 1 inserted']},
    {0: Diff.Diff.Operation.Delete, 1: ['line 2 deleted']},
  ];
  const uiSourceCodeMock = {mimeType: () => {}};

  TestRunner.addResult('Showing the Changes drawer.');
  await UI.ViewManager.ViewManager.instance().showView('changes.changes');
  const changesWidget = await UI.ViewManager.ViewManager.instance().view('changes.changes').widget();
  changesWidget.selectedUISourceCode = uiSourceCodeMock;
  changesWidget.renderDiffRows(diff);

  TestRunner.addResult('Running aXe on the Changes drawer.');
  await AxeCoreTestRunner.runValidation(changesWidget.contentElement);

  TestRunner.completeTest();
})();
