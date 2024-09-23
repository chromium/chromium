// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('../sources/debugger-breakpoints/resources/dom-breakpoints.html');

  await UI.ViewManager.ViewManager.instance().showView('sources.watch');
  TestRunner.addResult('Adding watch expression.');
  const watchPane = Sources.WatchExpressionsSidebarPane.WatchExpressionsSidebarPane.instance();
  watchPane.doUpdate();
  TestRunner.addResult('Running the axe-core linter on the empty watch pane.');
  await AxeCoreTestRunner.runValidation(watchPane.contentElement);

  const watchExpression = watchPane.createWatchExpression('2 + 2 === 5');
  await TestRunner.addSnifferPromise(Sources.WatchExpressionsSidebarPane.WatchExpression.prototype, 'createWatchExpression');

  const watchExpressionElement = watchExpression.treeElement().listItemElement;
  TestRunner.addResult(`Watch expression text content: ${watchExpressionElement.deepTextContent().trim()}`);
  TestRunner.addResult('Running the axe-core linter on the watch expression.');
  await AxeCoreTestRunner.runValidation(watchExpressionElement);
  TestRunner.completeTest();
})();
