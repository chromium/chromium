// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

const GC = async () => {
  await TestRunner.evaluateInPageAsync(`new Promise(resolve =>
    GCController.asyncCollectAll(resolve))`);
};

(async function () {
  TestRunner.addResult(`Verify that JavaScript SourceMap handle different sourcemap with overlapping sources.`);

  TestRunner.markStep('initialWorkspace');
  var snapshot = BindingsTestRunner.dumpWorkspace();

  TestRunner.markStep('attachFrame1');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame1', './resources/jssourcemaps-with-overlapping-sources/frame1.html', '_test_create-frame1.js'),
    BindingsTestRunner.waitForSourceMap('script1.js.map'),
  ]);
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('attachAnotherFrame1');
  await Promise.all([
    BindingsTestRunner.attachFrame('anotherFrame1', './resources/jssourcemaps-with-overlapping-sources/frame1.html', '_test_create-anotherFrame1.js'),
    BindingsTestRunner.waitForSourceMap('script1.js.map'),
  ]);
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('attachFrame2');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame2', './resources/jssourcemaps-with-overlapping-sources/frame2.html', '_test_create-frame2.js'),
    BindingsTestRunner.waitForSourceMap('script2.js.map')
  ]);
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('detachAnotherFrame1');
  await BindingsTestRunner.detachFrame('anotherFrame1', '_test_detach-anotherFrame1.js');
  await BindingsTestRunner.GC();
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('detachFrame2');
  await BindingsTestRunner.detachFrame('frame2', '_test_detachFrame2.js');
  await BindingsTestRunner.GC();
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('detachFrame1');
  await BindingsTestRunner.detachFrame('frame1', '_test_detachFrame1.js');
  await BindingsTestRunner.GC();
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.completeTest();
})();
