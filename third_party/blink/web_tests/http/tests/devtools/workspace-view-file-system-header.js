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
  const workspaceElement = (await UI.ViewManager.ViewManager.instance().view('workspace').widget()).element;

  const fsName = workspaceElement.querySelector('.file-system-name').textContent;
  const fsPath = workspaceElement.querySelector('.file-system-path').textContent;

  TestRunner.addResult(`File system name: ${fsName}`);
  TestRunner.addResult(`File system path: ${fsPath}`);
  TestRunner.completeTest();
})();
