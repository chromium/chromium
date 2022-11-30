// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Checks that JavaScriptSourceFrame show inline breakpoints correctly\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function foo()
      {
          var p = Promise.resolve().then(() => console.log(42))
              .then(() => console.log(239));
          return p;
      }

      // some comment.


      // another comment.




      function boo() {
        return 42;
      }

      console.log(42);
      //# sourceURL=foo.js
    `);

  Bindings.breakpointManager.storage._breakpoints = new Map();
  SourcesTestRunner.runDebuggerTestSuite([
    function testAddRemoveBreakpoint(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('foo.js', addBreakpoint);

      function addBreakpoint(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Setting breakpoint');
        // Breakpoint decoration expectations are pairs of line number plus breakpoint decoration counts.
        // We expect line 11 to have 5 decorations.
        SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(javaScriptSourceFrame, [[11, 5]], () =>
          SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 11, '', true)
        ).then(removeBreakpoint);
      }

      function removeBreakpoint() {
        TestRunner.addResult('Toggle breakpoint');
        SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(javaScriptSourceFrame, [], () =>
          SourcesTestRunner.toggleBreakpoint(javaScriptSourceFrame, 11)
        ).then(next);
      }
    },

    function testAddRemoveBreakpointInLineWithOneLocation(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('foo.js', addBreakpoint);

      function addBreakpoint(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Setting breakpoint');
        SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(javaScriptSourceFrame, [[13, 1]], () =>
          SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 13, '', true)
        ).then(removeBreakpoint);
      }

      function removeBreakpoint() {
        TestRunner.addResult('Toggle breakpoint');
        SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(javaScriptSourceFrame, [], () =>
          SourcesTestRunner.toggleBreakpoint(javaScriptSourceFrame, 13)
        ).then(next);
      }
    },

    function clickByInlineBreakpoint(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('foo.js', addBreakpoint);

      function addBreakpoint(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Setting breakpoint');
        SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(javaScriptSourceFrame, [[11, 5]], () =>
          SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 11, '', true)
        ).then(clickBySecondLocation);
      }

      function clickBySecondLocation() {
        TestRunner.addResult('Click by second breakpoint');
        SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(javaScriptSourceFrame, [[11, 5]], () =>
          SourcesTestRunner.clickDebuggerPluginBreakpoint(
              javaScriptSourceFrame, 11, 1, next)
        ).then(clickByFirstLocation);
      }

      function clickByFirstLocation() {
        TestRunner.addResult('Click by first breakpoint');
        SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(javaScriptSourceFrame, [[11, 5]], () =>
          SourcesTestRunner.clickDebuggerPluginBreakpoint(
              javaScriptSourceFrame, 11, 0, next)
        ).then(clickBySecondLocationAgain);
      }

      function clickBySecondLocationAgain() {
        TestRunner.addResult('Click by second breakpoint');
        SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(javaScriptSourceFrame, [], () =>
          SourcesTestRunner.clickDebuggerPluginBreakpoint(
              javaScriptSourceFrame, 11, 1, next)
        ).then(next);
      }
    },

    function toggleBreakpointInAnotherLineWontRemoveExisting(next) {
      var javaScriptSourceFrame;
      SourcesTestRunner.showScriptSource('foo.js', addBreakpoint);

      function addBreakpoint(sourceFrame) {
        javaScriptSourceFrame = sourceFrame;
        TestRunner.addResult('Setting breakpoint in line 4');
        SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(javaScriptSourceFrame, [[12, 3]], () =>
          SourcesTestRunner.toggleBreakpoint(javaScriptSourceFrame, 12, false)
        ).then(toggleBreakpointInAnotherLine);
      }

      function toggleBreakpointInAnotherLine() {
        TestRunner.addResult('Setting breakpoint in line 3');
        SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(javaScriptSourceFrame, [[11, 5], [12, 3]], () =>
          SourcesTestRunner.toggleBreakpoint(javaScriptSourceFrame, 11, false)
        ).then(removeBreakpoints);
      }

      function removeBreakpoints() {
        TestRunner.addResult('Click by first inline breakpoints');
        SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(javaScriptSourceFrame, [], () => {
          SourcesTestRunner.clickDebuggerPluginBreakpoint(
              javaScriptSourceFrame, 11, 0, next);
          SourcesTestRunner.clickDebuggerPluginBreakpoint(
              javaScriptSourceFrame, 12, 0, next);
        }).then(next);
      }
    },

    async function testAddRemoveBreakpointInLineWithoutBreakableLocations(next) {
      let javaScriptSourceFrame = await SourcesTestRunner.showScriptSourcePromise('foo.js');

      TestRunner.addResult('Setting breakpoint');
      await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(javaScriptSourceFrame, [[28, 2]], () =>
        SourcesTestRunner.createNewBreakpoint(javaScriptSourceFrame, 16, '', true)
      );

      TestRunner.addResult('Toggle breakpoint');
      await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(javaScriptSourceFrame, [], () =>
        SourcesTestRunner.toggleBreakpoint(javaScriptSourceFrame, 28)
      );
      next();
    }
  ]);
})();
