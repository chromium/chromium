// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult('Tests workspace view file system headers\n');
  const fs = new BindingsTestRunner.TestFileSystem('/this/is/a/test');
  await fs.reportCreatedPromise();

  await UI.ViewManager.ViewManager.instance().showView('workspace');
  const workspaceElement = (await UI.ViewManager.ViewManager.instance().view('workspace').widget()).contentElement;

  // The first card contains the general folder exclude pattern.
  // Starting from the second card, we list the specific folders to be excluded.
  const card = workspaceElement.querySelectorAll('devtools-card')[1];
  const fsName = card?.shadowRoot.querySelector('.heading').textContent;

  const mappingView = card.querySelector('.file-system-mapping-view');
  const fsPath = mappingView?.shadowRoot?.querySelector('.excluded-folder-url')?.textContent;

  TestRunner.addResult(`File system name: ${fsName}`);
  TestRunner.addResult(`File system path: ${fsPath}`);
  TestRunner.completeTest();
})();
