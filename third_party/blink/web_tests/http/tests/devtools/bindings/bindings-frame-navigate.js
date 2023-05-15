// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

(async function() {
  TestRunner.addResult(`Verify that UISourceCodes are removed as the frame gets navigated.\n`);

  TestRunner.markStep('dumpInitialWorkspace');
  var snapshot = BindingsTestRunner.dumpWorkspace();

  TestRunner.markStep('attachFrame');
  await BindingsTestRunner.attachFrame('frame', './resources/magic-frame.html', '_test_attachFrame.js');
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('navigateFrame');
  await BindingsTestRunner.navigateFrame('frame', './resources/empty-frame.html', '_test_navigateFrame');
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.completeTest();
})();
