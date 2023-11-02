// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function assertLocation(section, select_row) {
  const sectionRow = await HeapProfilerTestRunner.findAndExpandRow(section);
  const instanceRow = sectionRow.children[0];
  await HeapProfilerTestRunner.expandRowPromise(instanceRow);
  let rowWithLocation = instanceRow;

  if (select_row) {
    rowWithLocation = HeapProfilerTestRunner.findMatchingRow(n => n.referenceName === select_row, rowWithLocation);
  }

  let linkNode;
  do {
    linkNode = rowWithLocation.element().querySelector('.heap-object-source-link .devtools-link');
    await new Promise(r => requestAnimationFrame(r));
  } while (!linkNode);

  TestRunner.addResult(`source: ${linkNode.textContent}`);
}

(async function() {
  TestRunner.addResult(`Test that objects have source links in heap snapshot view.\n`);

  await TestRunner.loadTestModule('heap_profiler_test_runner');
  await TestRunner.showPanel('heap_profiler');
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
      window.myTestClass2 = new MyTestClass2();
      //# sourceURL=my-test-script.js`);

  await HeapProfilerTestRunner.takeSnapshotPromise();
  await HeapProfilerTestRunner.switchToView('Summary');

  const rowsToCheck = [
    ['MyTestClass', 'myFunction'],
    ['myGenerator'],
    ['MyTestClass2']
  ];
  for (let expected of rowsToCheck) {
    const section = expected[0];
    const select_row = expected[1];
    await assertLocation(section, select_row);
  }

  TestRunner.completeTest();
})();
