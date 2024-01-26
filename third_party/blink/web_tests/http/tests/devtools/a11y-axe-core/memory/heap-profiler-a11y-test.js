// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult('Tests accessibility in heap profiler using the axe-core linter.');
  await TestRunner.showPanel('heap-profiler');
  await TestRunner.evaluateInPagePromise(`
      class MyTestClass {
        constructor() {
          this.z = new Uint32Array(10000);  // Pull the class to top.
          this.myFunction = () => 42;
        }
      };
      function* myGenerator() {
        yield 1;
      }
      class MyTestClass2 {}
      window.myTestClass = new MyTestClass();
      window.myTestGenerator = myGenerator();
      window.myTestClass2 = new MyTestClass2();`);

  await HeapProfilerTestRunner.startSamplingHeapProfiler();
  await TestRunner.evaluateInPagePromise(`
      function pageFunction() {
        (function () {
          window.holder = [];
          // Allocate few MBs of data.
          for (var i = 0; i < 1000; ++i)
            window.holder.push(new Array(1000).fill(42));
        })();
      }
      pageFunction();`);
  HeapProfilerTestRunner.stopSamplingHeapProfiler();

  await UI.ViewManager.ViewManager.instance().showView('heap-profiler');
  const widget = await UI.ViewManager.ViewManager.instance().view('heap-profiler').widget();
  await AxeCoreTestRunner.runValidation(widget.element);
  TestRunner.completeTest();
})();
