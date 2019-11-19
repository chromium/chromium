// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that text prompt suggestions' casing follows that of the user input.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <div id="inner" style="color:initial;"></div>
  `);

  await ElementsTestRunner.selectNodeAndWaitForStylesPromise('inner');
  var colorTreeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('color');
  var prompt = new Elements.StylesSidebarPane.CSSPropertyPrompt(colorTreeElement, true /* isEditingName */);

  TestRunner.runTestSuite([
    function testForUpperCase(next) {
      testAutoCompletionsAgainstCase(prompt, 'C', next);
    },

    function testForLowerCase(next) {
      testAutoCompletionsAgainstCase(prompt, 'b', next);
    },

    function testForMixedCase(next) {
      testAutoCompletionsAgainstCase(prompt, 'bAcK', next);
    }
  ]);

  function testAutoCompletionsAgainstCase(prompt, inputText, callback) {
    var proxyElement = document.body.createChild('span');
    proxyElement.textContent = inputText;
    var selectionRange = document.createRange();
    selectionRange.selectNodeContents(proxyElement);
    var prefix = selectionRange.toString();
    prompt._buildPropertyCompletions(inputText.substring(0, inputText.length - prefix.length), prefix, true)
        .then(completions);

    function completions(result) {
      function isUpperCase(str) {
        return str === str.toUpperCase();
      }

      function isLowerCase(str) {
        return str === str.toLowerCase();
      }

      var Case = {Upper: 0, Lower: 1, Mixed: 2};

      var inputCase = isUpperCase(inputText) ? Case.Upper : isLowerCase(inputText) ? Case.Lower : Case.Mixed;

      for (var i = 0; i < result.length; ++i) {
        let text = result[i].text;
        const colonIndex = text.indexOf(':');
        if (colonIndex !== -1)
          text = text.substring(0, colonIndex);
        switch (inputCase) {
          case Case.Upper:
            if (!isUpperCase(text))
              TestRunner.addResult('Error: Suggestion ' + text + ' must be in UPPERCASE.');
            break;
          case Case.Lower:
            if (!isLowerCase(text))
              TestRunner.addResult('Error: Suggestion ' + text + ' must be in lowercase.');
            break;
        }
      }
      proxyElement.remove();
      callback();
    }
  }
})();
