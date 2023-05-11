// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

(async function () {
  TestRunner.addResult(`Verify that SourceMap bindings are generating UISourceCodes properly.\n`);

  TestRunner.markStep('initialWorkspace');
  var snapshot = BindingsTestRunner.dumpWorkspace();

  TestRunner.markStep('attachFrame1');
  await BindingsTestRunner.attachFrame('frame1', './resources/contentscript-frame.html', '_test_attachFrame1.js');
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('attachFrame2');
  await BindingsTestRunner.attachFrame('frame2', './resources/contentscript-frame.html', '_test_attachFrame2.js');
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('detachFrame1');
  await BindingsTestRunner.detachFrame('frame1', '_test_detachFrame1.js');
  await TestRunner.evaluateInPageAnonymously('GCController.collectAll()');
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('detachFrame2');
  await BindingsTestRunner.detachFrame('frame2', '_test_detachFrame2.js');
  await TestRunner.evaluateInPageAnonymously('GCController.collectAll()');
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.completeTest();
})();
