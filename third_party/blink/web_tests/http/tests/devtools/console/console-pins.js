// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console can pin expressions.\n`);

  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.loadHTML(`
    <div tabIndex="-1" id="div1">foo 1</div>
    <div tabIndex="-1" id="div2">foo 2</div>
    <script>
      window.flag = false;
    </script>
  `);
  await TestRunner.evaluateInPagePromise(`div1.focus()`);
  const consoleView = Console.ConsoleView.instance();
  const pinPane = consoleView._pinPane;

  TestRunner.runTestSuite([
    async function testBeforeAdding(next) {
      await dumpPinPaneContents();
      next();
    },

    async function testAddingExpression(next) {
      pinPane.addPin(`document.activeElement`);
      await waitForEditors();
      await waitForPinUpdate();
      await dumpPinPaneContents();
      next();
    },

    async function testPinUpdatesDynamically(next) {
      TestRunner.addResult(`Focusing the div2 on target page.`);
      await TestRunner.evaluateInPagePromise(`div2.focus()`);
      await waitForPinUpdate();
      await dumpPinPaneContents();
      next();
    },

    async function testNoSideEffectsWhileEditing(next) {
      TestRunner.addResult(`Focusing the first pin's editor.`);
      await pinAt(0).focus();

      const sideEffectExpression = `window.flag = true`;
      TestRunner.addResult(`Setting text to: "${sideEffectExpression}".`);
      pinAt(0)._editor.setText(sideEffectExpression);

      await waitForPinUpdate();
      await dumpPinPaneContents();
      const flagResult = await TestRunner.evaluateInPagePromise(`window.flag`);
      TestRunner.addResult(`window.flag is now: ${flagResult}`);

      pinAt(0)._editor.setText(`document.activeElement`);
      next();
    },

    async function testRemoveSinglePin(next) {
      pinPane.addPin(`"Second pin"`);
      await waitForEditors();
      await waitForPinUpdate();
      await dumpPinPaneContents();

      TestRunner.addResult(`\nRemoving second pin\n`);
      pinPane._removePin(pinAt(1));
      await dumpPinPaneContents();
      next();
    },

    async function testRemoveAllPins(next) {
      pinPane._removeAllPins();
      await dumpPinPaneContents();
      next();
    }
  ]);

  async function dumpPinPaneContents() {
    if (!pinPane._pins.size) {
      TestRunner.addResult(`No pins`);
      return;
    }
    for (const pin of pinPane._pins)
      TestRunner.addResult(`Name: ${pin._editor.text()}\nValue: ${pin._pinPreview.deepTextContent()}`);
  }

  async function waitForEditors() {
    for (const pin of pinPane._pins)
      await pin._editorPromise;
  }

  async function waitForPinUpdate(index) {
    await TestRunner.addSnifferPromise(pinPane, '_updatedForTest');
  }

  function pinAt(index) {
    return Array.from(pinPane._pins)[index];
  }
})();
