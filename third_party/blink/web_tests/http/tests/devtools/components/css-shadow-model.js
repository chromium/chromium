// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests CSSLength.parse, CSSShadowModel.parseTextShadow, and CSSShadowModel.parseBoxShadow.\n`);
  await TestRunner.loadLegacyModule("inline_editor");

  TestRunner.addResult('-----CSSLengths-----');
  dumpCSSLength('10px');
  dumpCSSLength('10PX');
  dumpCSSLength('-10px');
  dumpCSSLength('+10px');
  dumpCSSLength('10.11px');
  dumpCSSLength('.11px');
  dumpCSSLength('10e3px');
  dumpCSSLength('10E3px');
  dumpCSSLength('10.11e3px');
  dumpCSSLength('-10.11e-3px');
  dumpCSSLength('0px');
  dumpCSSLength('0');
  dumpCSSLength('-0.0');
  dumpCSSLength('+0.0');
  dumpCSSLength('0e-3');
  // Start invalid lengths.
  dumpCSSLength('');
  dumpCSSLength('10');
  dumpCSSLength('10 px');
  dumpCSSLength('10.px');
  dumpCSSLength('10pxx');
  dumpCSSLength('10.10.10px');
  dumpCSSLength('hello10pxhello');

  TestRunner.addResult('\n-----Text Shadows-----');
  dumpTextShadow('0 0');
  dumpTextShadow('1px 2px');
  dumpTextShadow('1px 2px black');
  dumpTextShadow('1px 2px 2px');
  dumpTextShadow('rgb(0, 0, 0) 1px 2px 2px');
  dumpTextShadow('1px 2px 2px rgb(0, 0, 0)');
  dumpTextShadow('1px 2px black, 0 0 #ffffff');
  dumpTextShadow('1px -2px black, 0 0 rgb(0, 0, 0), 3px 3.5px 3px');
  // Start invalid text shadows.
  dumpTextShadow('');
  dumpTextShadow('0');
  dumpTextShadow('1 2 black');
  dumpTextShadow('1px black 2px');
  dumpTextShadow('1px 2px 2px 3px');
  dumpTextShadow('inset 1px 2px 2px');
  dumpTextShadow('red 1px 2px 2px red');
  dumpTextShadow('1px 2px rgb(0, 0, 0) 2px');
  dumpTextShadow('hello 1px 2px');
  dumpTextShadow('1px 2px black 0 0 #ffffff');
  dumpTextShadow('1px2px');
  dumpTextShadow('1px 2pxrgb(0, 0, 0)');
  dumpTextShadow('1px 2px black,, 0 0 #ffffff');

  TestRunner.addResult('\n-----Box Shadows-----');
  dumpBoxShadow('0 0');
  dumpBoxShadow('1px 2px');
  dumpBoxShadow('1px 2px black');
  dumpBoxShadow('1px 2px 2px');
  dumpBoxShadow('1px 2px 2px 3px');
  dumpBoxShadow('inset 1px 2px');
  dumpBoxShadow('1px 2px inset');
  dumpBoxShadow('INSET 1px 2px 2px 3px');
  dumpBoxShadow('rgb(0, 0, 0) 1px 2px 2px');
  dumpBoxShadow('inset rgb(0, 0, 0) 1px 2px 2px');
  dumpBoxShadow('inset 1px 2px 2px 3px rgb(0, 0, 0)');
  dumpBoxShadow('1px 2px 2px 3px rgb(0, 0, 0) inset');
  dumpBoxShadow('1px 2px black, inset 0 0 #ffffff');
  dumpBoxShadow('1px -2px black, inset 0 0 rgb(0, 0, 0), 3px 3.5px 3px 4px');
  // Start invalid box shadows.
  dumpBoxShadow('');
  dumpBoxShadow('0');
  dumpBoxShadow('1 2 black');
  dumpBoxShadow('1px black 2px');
  dumpBoxShadow('1px 2px 2px 3px 4px');
  dumpBoxShadow('1px 2px 2px inset 3px');
  dumpBoxShadow('inset 1px 2px 2px inset');
  dumpBoxShadow('1px 2px rgb(0, 0, 0) 2px');
  dumpBoxShadow('hello 1px 2px');
  dumpBoxShadow('1px 2px black 0 0 #ffffff');
  dumpBoxShadow('1px2px');
  dumpBoxShadow('1px 2pxrgb(0, 0, 0)');
  dumpBoxShadow('1px 2px black,, 0 0 #ffffff');

  TestRunner.completeTest();

  function dumpCSSLength(lengthText) {
    var length = InlineEditor.CSSLength.parse(lengthText);
    var statusText = length !== null ? 'Succeeded: ' + length.asCSSText() : 'Failed';
    TestRunner.addResult('"' + lengthText + '", Parsing ' + statusText);
  }

  function dumpTextShadow(shadowText) {
    dumpShadow(shadowText, false);
  }

  function dumpBoxShadow(shadowText) {
    dumpShadow(shadowText, true);
  }

  function dumpShadow(shadowText, isBoxShadow) {
    var shadows = isBoxShadow ? InlineEditor.CSSShadowModel.parseBoxShadow(shadowText) :
                                InlineEditor.CSSShadowModel.parseTextShadow(shadowText);
    var output = [];
    for (var i = 0; i < shadows.length; i++)
      output.push(shadows[i].asCSSText());
    var statusText = shadows.length !== 0 ? 'Succeeded: ' + output.join(', ') : 'Failed';
    TestRunner.addResult('"' + shadowText + '", Parsing ' + statusText);
  }
})();
