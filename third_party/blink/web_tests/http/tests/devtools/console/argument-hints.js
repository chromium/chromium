// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests function argument hints.\n`);

  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
     const boundAddClickListener = document.addEventListener.bind(document, 'click');
     function userFunctionWithDefault(required, notRequired = 5) {

     }

     function userFunctionWithDestructuring({something1}, [sommething2, something3]) {

     }

     function originalFunction(a,b,c,...more) {

     }

     const secondFunction = originalFunction.bind(null, 'a');
     const thirdFunction = secondFunction.bind(null, 'b');
     const fourthFunction = thirdFunction.bind(null, 'c');
     const fifthFunction = fourthFunction.bind(null, 'more');
     const aLotBound = originalFunction.bind(null, 'a','b','c','d','e','f');

     const ctx = document.createElement('canvas').getContext('2d');
     const boundMathLog = Math.log.bind(Math);

     class Thing {
       constructor(initialValue) {

       }

       static myStaticMethod(a,b,c) {

       }

       setT(t) {

       }
     }

     const aString = "aString";
     const aNumber = 4;
  `);
  const consoleEditor = await ConsoleTestRunner.waitUntilConsoleEditorLoaded();
  await testHints('open(');
  await testHints('Math.log(');
  await testHints('console.log(1,');
  await testHints('const process = window.setTimeout(');
  await testHints('document.body.addEventListener(');
  await testHints('boundAddClickListener(x => x,');
  await testHints('userFunctionWithDestructuring(');
  await testHints('originalFunction(a,b,c,d,e,f,g,');
  await testHints('secondFunction(');
  await testHints('thirdFunction(');
  await testHints('fourthFunction(');
  await testHints('fifthFunction(');
  await testHints('aLotBound(');
  await testHints('boundMathLog(');
  await testHints('CSS.supports(');
  await testHints('Uint8Array.from(');
  await testHints('ctx.drawImage(image, x, y,');
  await testHints('window.open(document.getElementsByName(|));');
  await testHints('window.open(|document.getElementsByName());');
  await testHints('"asdf".indexOf(');
  await testHints('[].indexOf(');
  await testHints('new Thing(');
  await testHints('(new Thing).setT(');
  await testHints('(x => x)(');
  await testHints('(function(y){})(');
  await testHints('Thing.myStaticMethod(');
  await testHints('aString.toString(');
  await testHints('aNumber.toString(');
  await testHints('[1,2,3].splice(');
  await testHints('URL.createObjectURL(');
  await testHints('(() => window)().URL["revokeObjectURL"](');
  await testHints('var notInAfunction');
  await testHints('some gibberish $@#)(*^@#');
  await testHints('Date.parse(');
  await testHints('JSON.parse(');
  await testHints('CSSNumericValue.parse(');
  TestRunner.completeTest();

  /**
   * @param {string} text
   * @param {!Array<string>} expected
   */
  async function testHints(text, expected) {
    var cursorPosition = text.indexOf('|');

    if (cursorPosition < 0)
      cursorPosition = Infinity;

    consoleEditor.setText(text.replace('|', ''));
    consoleEditor.setSelection(
        TextUtils.TextRange.createFromLocation(0, cursorPosition));
    const signaturesPromise = new Promise(
        x => TestRunner.addSniffer(
            ObjectUI.javaScriptAutocomplete, 'argumentsHint',
            (text, retVal) => x(retVal)));
    consoleEditor._autocompleteController._onCursorActivity();
    var message = 'Checking \'' + +'\'';

    const signatures = await signaturesPromise;
    TestRunner.addResult(`${
                            text.replace('\n', '\\n').replace('\r', '\\r')
                          }\n${printSignatures()}`);
    TestRunner.addResult('');
    function printSignatures() {
      if (!signatures)
        return 'null';
      return signatures.args
          .map(args => {
            return args
                .map((arg, index) => {
                  return arg;
                })
                .join(',');
          })
          .join('\n')
    }
  }
})();
