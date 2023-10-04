// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as Sources from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult('Testing accessibility in the threads sidebar pane.');

  await TestRunner.showPanel('sources');
  await SourcesTestRunner.startDebuggerTestPromise(/* quiet */ true);

  await TestRunner.evaluateInPagePromise(`new Worker('../../sources/resources/worker-source.js')`);
  await SourcesTestRunner.waitUntilPausedPromise();
  const sourcesPanel = Sources.SourcesPanel.SourcesPanel.instance();
  sourcesPanel.showThreadsIfNeeded();
  await UI.ViewManager.ViewManager.instance().showView('sources.threads');

  const threadsSidebarPane = await sourcesPanel.threadsSidebarPane.widget();
  const threadsSidebarElement = threadsSidebarPane.contentElement;
  TestRunner.addResult(`Threads sidebar pane content:\n ${threadsSidebarElement.deepTextContent()}`);
  TestRunner.addResult('Running the axe-core linter on the threads sidebar pane.');
  await AxeCoreTestRunner.runValidation(threadsSidebarElement);
  TestRunner.completeTest();

})();
