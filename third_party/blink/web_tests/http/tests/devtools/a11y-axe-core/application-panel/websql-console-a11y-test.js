// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

(async function() {
  TestRunner.addResult(`Tests accessibility of WebSQL console in application panel.`);
  await TestRunner.loadLegacyModule('console');
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  async function setPromptText(text) {
    queryView.prompt.setText(text);
    await queryView.enterKeyPressed(new KeyboardEvent('keydown'));
  }

  UI.viewManager.showView('resources');
  await TestRunner.evaluateInPagePromise(
    'openDatabase("inspector-test-db", "1.0", "Database for inspector test", 1024*1024)');

  const parent = UI.panels.resources.sidebar.sidebarTree.rootElement();
  const databaseElement = ApplicationTestRunner.findTreeElement(parent, ['Storage', 'Web SQL', 'inspector-test-db']);
  databaseElement.select();

  const queryView = UI.panels.resources.visibleView;

  await setPromptText('CREATE TABLE table1 (id INTEGER PRIMARY KEY ASC, text_field TEXT)');
  await setPromptText('INSERT INTO table1 (id, text_field) VALUES (1, "foobar")');
  await setPromptText('SELECT * FROM table1');
  await setPromptText('invalid input');

  await AxeCoreTestRunner.runValidation(queryView.element);
  TestRunner.completeTest();
})();