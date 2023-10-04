// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Tests that continue to location markers are computed correctly.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
        function foo1() {
          return 10;
        }

        function foo2() {
          return {a:x => 2 * x};
        }

        async function bar1() {
          return 10;
        }

        async function bar2(x) {
          return 2 * x;
        }

        async function foo3() {
          debugger;
          var a = foo1() + foo1();
          var b = foo2();
          if (a) {
            a = b.a(a);
          }

          bar1().then((xxx, yyy) => console.log(xxx));
          bar1().then(     (xxx, yyy) => console.log(xxx));
          bar1().then(     (xxx, /*zzz*/ yyy /* xyz    */) => console.log(xxx));
          bar1().then   (     bar2    );
          bar1().then   (     console.log()    );
          bar1().then   (     console.log    );
          bar1().then(function(x) {
            console.log(x);
          });
          bar1().then(   async /* comment */  function(x) {
            console.log(x);
          });
          bar1().then(   async function(x) {
            console.log(x);
          });
          bar1().then(bar2.bind(null));
          bar1().then(() => bar2(5));
          bar1().then(async () => await bar2(5));
          bar1().then(async (x, y) => await bar2(x));
          setTimeout(bar1, 2000);
          a = await bar1();
          bar1().then((x,
                       y) => console.log(x));
          bar1().then((
              x, y) => console.log(x));
          bar1().then(async (
              x, y) => console.log(x));
          bar1().then(
              async (x, y) => console.log(x));
          bar1().then(
              bar2);
          bar1().then((bar2));
          bar1().then(Promise.resolve());
          bar1().then(Promise.resolve(42).then(bar2));
          bar1().then((Promise.resolve()));

          var False = false;
          if (False)
            bar1().then(bar2);
          bar1().then(bar2);

          bar1().then(/* comment */ bar2.bind(null));

          let blob = new Blob([''], {type: 'application/javascript'});
          let worker = new Worker(URL.createObjectURL(blob));
          worker.postMessage('hello!');

          return 10;
        }
    `);
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
        foo3();
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    TestRunner
        .addSnifferPromise(
            Sources.DebuggerPlugin.DebuggerPlugin.prototype,
            '_continueToLocationRenderedForTest')
        .then(step2);
    TestRunner.addSniffer(
        Sources.DebuggerPlugin.DebuggerPlugin.prototype, '_executionLineChanged', function() {
          SourcesTestRunner.showUISourceCodePromise(this._uiSourceCode)
              .then(() => {
                this._showContinueToLocations();
              });
        });
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused();
  }

  function step2() {
    var currentFrame = Sources.SourcesPanel.SourcesPanel.instance().visibleView;
    var debuggerPlugin = SourcesTestRunner.debuggerPlugin(currentFrame);
    var decorations = debuggerPlugin._continueToLocationDecorations;
    var lines = [];
    for (var decoration of decorations.keys()) {
      var find = decoration.find();
      var line = find.from.line;
      var text = currentFrame.textEditor.line(line).substring(find.from.ch, find.to.ch);
      lines.push(`${decoration.className} @${line + 1}:${find.from.ch} = '${text}'`);
    }
    lines.sort();
    TestRunner.addResults(lines);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
