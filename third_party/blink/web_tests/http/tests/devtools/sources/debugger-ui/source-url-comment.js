// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that evals with sourceURL comment are shown in scripts panel.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('resources/source-url-comment.html');
  await TestRunner.evaluateInPagePromise(`
      function doEval()
      {
          eval("function keepAlive() {}\\n//# sourceURL=evalURL.js");
      }

      function doEvalWithNonRelativeURL()
      {
          eval("function relativeURLScript() {}\\n//# sourceURL=js/nonRelativeURL.js");
      }

      function doDynamicScript()
      {
          var scriptElement = document.createElement("script");
          scriptElement.textContent = "function keepAliveInDynamicScript() {}\\n//# sourceURL=dynamicScriptURL.js";
          document.body.appendChild(scriptElement);
      }

      function doURLAndMappingURL()
      {
          eval("function keepAlive() {}\\n//# sourceMappingURL=sourceMappingURL.map\\n//# sourceURL=sourceURL.js");
      }

      function doEvalWithMultipleSourceURL()
      {
          eval("\\n//# sourceURL=evalURL2.js\\nfunction keepAlive() {}\\n//# sourceURL=evalMultipleURL.js");
      }
  `);

  function forEachScriptMatchingURL(url, handler) {
    for (var script of TestRunner.debuggerModel.scripts()) {
      if (script.sourceURL.indexOf(url) !== -1)
        handler(script);
    }
  }

  SourcesTestRunner.runDebuggerTestSuite([
    function testSourceURLCommentInInlineScript(next) {
      SourcesTestRunner.showScriptSource(
          'source-url-comment.html', didShowScriptSource);

      function didShowScriptSource(sourceFrame) {
        var panel = UI.panels.sources;
        var uiSourceCodes = panel._workspace.uiSourceCodes();
        var ignored = true;
        for (var i = 0; i < uiSourceCodes.length && ignored; ++i) {
          if (uiSourceCodes[i].url().indexOf('inlineScriptURL.js') !== -1)
            ignored = false;
        }
        if (ignored)
          TestRunner.addResult(
              'FAILED: sourceURL comment in inline script was ignored');
        next();
      }
    },

    function testSourceURLCommentInScript(next) {
      SourcesTestRunner.showScriptSource(
          'scriptWithSourceURL.js', didShowScriptSource);
      TestRunner.addScriptTag('../../debugger/resources/script-with-url.js');

      function didShowScriptSource(sourceFrame) {
        TestRunner.addResult(sourceFrame.textEditor.text().trim());
        forEachScriptMatchingURL(
            'scriptWithSourceURL.js', checkScriptSourceURL);
        next();
      }
    },

    function testPoorSourceURLCommentInScript(next) {
      SourcesTestRunner.showScriptSource(
          'source-url-comment.html', didShowScriptSource);
      TestRunner.addScriptTag(
          '../../debugger/resources/script-with-poor-url.js');

      function didShowScriptSource(sourceFrame) {
        var panel = UI.panels.sources;
        var uiSourceCodes = panel._workspace.uiSourceCodes();
        for (var i = 0; i < uiSourceCodes.length; ++i) {
          if (uiSourceCodes[i].url().indexOf('scriptWithPoorSourceURL.js') !==
              -1)
            TestRunner.addResult(
                'FAILED: poor sourceURL comment in script was used as a script name');
        }
        next();
      }
    },

    function testSourceURLComment(next) {
      SourcesTestRunner.showScriptSource('evalURL.js', didShowScriptSource);
      TestRunner.evaluateInPage('setTimeout(doEval, 0)');

      function didShowScriptSource(sourceFrame) {
        TestRunner.addResult(sourceFrame.textEditor.text());
        forEachScriptMatchingURL('evalURL.js', checkScriptSourceURL);
        next();
      }
    },

    function testSourceURLAndMappingURLComment(next) {
      SourcesTestRunner.showScriptSource('sourceURL.js', didShowScriptSource);
      TestRunner.evaluateInPage('setTimeout(doURLAndMappingURL, 0)');

      function didShowScriptSource(sourceFrame) {
        function checkScriptSourceURLAndMappingURL(script) {
          TestRunner.addResult('hasSourceURL: ' + script.hasSourceURL);
          TestRunner.addResult('sourceMapURL: ' + script.sourceMapURL);
        }

        TestRunner.addResult(sourceFrame.textEditor.text());
        forEachScriptMatchingURL(
            'sourceURL.js', checkScriptSourceURLAndMappingURL);
        next();
      }
    },

    function testSourceURLCommentInDynamicScript(next) {
      SourcesTestRunner.showScriptSource(
          'dynamicScriptURL.js', didShowScriptSource);
      TestRunner.evaluateInPage('setTimeout(doDynamicScript, 0)');

      function didShowScriptSource(sourceFrame) {
        TestRunner.addResult(sourceFrame.textEditor.text());
        forEachScriptMatchingURL('dynamicScriptURL.js', checkScriptSourceURL);
        next();
      }
    },

    function testNonRelativeURL(next) {
      SourcesTestRunner.showScriptSource(
          'js/nonRelativeURL.js', didShowScriptSource);
      TestRunner.evaluateInPage('setTimeout(doEvalWithNonRelativeURL, 0)');

      function didShowScriptSource(sourceFrame) {
        TestRunner.addResult(sourceFrame.textEditor.text());
        forEachScriptMatchingURL('nonRelativeURL.js', checkScriptSourceURL);
        next();
      }
    },

    function testMultipleSourceURLComment(next) {
      SourcesTestRunner.showScriptSource(
          'evalMultipleURL.js', didShowScriptSource);
      TestRunner.evaluateInPage('setTimeout(doEvalWithMultipleSourceURL, 0)');

      function didShowScriptSource(sourceFrame) {
        TestRunner.addResult(sourceFrame.textEditor.text());
        forEachScriptMatchingURL('evalMultipleURL.js', checkScriptSourceURL);
        next();
      }
    }
  ]);

  function checkScriptSourceURL(script) {
    TestRunner.addResult('hasSourceURL: ' + script.hasSourceURL);
  }
})();
