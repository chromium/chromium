// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test verifies Ctrl-D functionality, which selects next occurrence of word.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
<pre id="codeSnippet">function wordData() {
    return {
        original: $(&quot;.entry.original &gt; .input&quot;).text(),
        translation: $(&quot;.entry.translation &gt; .input&quot;).text(),
        tags: $(&quot;.active-tags &gt; .tagcloud &gt; .tag&quot;).toArray().map(function(value) { return value.textContent; })
    };
}

function submitWord(url) {
    var stub = new App.Stub($(&quot;.content&quot;));
    $.post(url, wordData())
    .done(function() {
        var callback = $(&quot;meta[data-callback]&quot;).attr(&quot;data-callback&quot;);
        if (callback) {
            window.location = callback;
        } else {
            stub.success();
            $(&quot;.entry.original &gt; .input&quot;).text(&quot;&quot;).focus();
            $(&quot;.entry.translation &gt; .input&quot;).text(&quot;&quot;);
        }
    })
    .fail(function(obj, err, errDescr) {
        stub.failure(&quot;Error: &quot; + errDescr);
    })
}
</pre>
    `);
  await TestRunner.dumpInspectedPageElementText('#codeSnippet');
  await TestRunner.evaluateInPagePromise(`
      function codeSnippet() {
          return document.getElementById("codeSnippet").textContent;
      }
  `);

  var textEditor = SourcesTestRunner.createTestEditor();
  textEditor.setMimeType('text/javascript');
  textEditor.setReadOnly(false);
  textEditor.element.focus();
  TestRunner.evaluateInPage('codeSnippet();', onCodeSnippet);

  function onCodeSnippet(result) {
    var codeLines = result;
    textEditor.setText(codeLines);
    TestRunner.runTestSuite(testSuite);
  }

  function nextOccurrence(times) {
    for (var i = 0; i < times; ++i)
      textEditor.selectNextOccurrenceController.selectNextOccurrence();
  }

  function undoLastSelection() {
    textEditor.selectNextOccurrenceController.undoLastSelection();
  }

  function lineSelection(line, from, to) {
    if (typeof to !== 'number')
      to = from;
    SourcesTestRunner.setLineSelections(textEditor, [{line: line, from: from, to: to}]);
  }

  var testSuite = [
    function testNextFullWord(next) {
      lineSelection(0, 3);
      nextOccurrence(3);
      SourcesTestRunner.dumpSelectionStats(textEditor);
      next();
    },

    function testCaseSensitive(next) {
      lineSelection(0, 9, 13);
      nextOccurrence(3);
      SourcesTestRunner.dumpSelectionStats(textEditor);
      next();
    },

    function testOccurrencesOnTheSameLine(next) {
      lineSelection(2, 13);
      nextOccurrence(3);
      SourcesTestRunner.dumpSelectionStats(textEditor);
      next();
    },

    function testUndoLastAddedSelection(next) {
      lineSelection(2, 13);
      nextOccurrence(3);
      undoLastSelection();
      SourcesTestRunner.dumpSelectionStats(textEditor);
      next();
    },

    function testUndoSelectionPreservesFullWordState(next) {
      lineSelection(2, 51);
      nextOccurrence(3);
      undoLastSelection();
      nextOccurrence(1);
      SourcesTestRunner.dumpSelectionStats(textEditor);
      var lastSelection = textEditor.selections().pop();
      TestRunner.addResult('Last selection: ' + lastSelection.toString());
      next();
    },

    function testUndoSelectionPreservesPartialSelection(next) {
      lineSelection(2, 48, 52);
      nextOccurrence(2);
      undoLastSelection();
      nextOccurrence(1);
      SourcesTestRunner.dumpSelectionStats(textEditor);
      var lastSelection = textEditor.selections().pop();
      TestRunner.addResult('Last selection: ' + lastSelection.toString());
      next();
    },

    function testTwoCloseWords(next) {
      lineSelection(17, 45);
      nextOccurrence(5);
      SourcesTestRunner.dumpSelectionStats(textEditor);
      next();
    },
  ];
})();
