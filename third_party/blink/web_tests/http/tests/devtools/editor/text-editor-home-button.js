// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `This test verifies that home button triggers selection between first symbol of the line and first non-blank symbol of the line.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  // clang-format off
function foo()
{
    return 42;
}
  // clang-format on
  var textEditor = SourcesTestRunner.createTestEditor();
  textEditor.setMimeType('text/javascript');
  textEditor.setReadOnly(false);
  textEditor.element.focus();

  textEditor.setText(foo.toString());

  TestRunner.addResult(textEditor.text());

  function homeButton(shift, callback) {
    var key = Host.isMac() ? 'ArrowLeft' : 'Home';
    var modifiers = Host.isMac() ? ['metaKey'] : [];
    if (shift)
      modifiers.push('shiftKey');
    SourcesTestRunner.fakeKeyEvent(textEditor, key, modifiers, callback);
  }

  function hitHomeButton(shift, times, callback) {
    function hitButtonCallback() {
      --times;
      SourcesTestRunner.dumpTextWithSelection(textEditor);
      if (times > 0) {
        homeButton(shift, hitButtonCallback);
        return;
      }
      callback();
    }
    homeButton(shift, hitButtonCallback);
  }

  TestRunner.runTestSuite([
    function testFirstNonBlankCharacter(next) {
      var selection = TextUtils.TextRange.createFromLocation(2, 8);
      textEditor.setSelection(selection);
      SourcesTestRunner.dumpTextWithSelection(textEditor);
      hitHomeButton(false, 1, next);
    },

    function testFirstNonBlankCharacterFromWhitespace(next) {
      var selection = TextUtils.TextRange.createFromLocation(2, 2);
      textEditor.setSelection(selection);
      SourcesTestRunner.dumpTextWithSelection(textEditor);
      hitHomeButton(false, 1, next);
    },

    function testHomeButtonToggling(next) {
      var selection = TextUtils.TextRange.createFromLocation(2, 2);
      textEditor.setSelection(selection);
      SourcesTestRunner.dumpTextWithSelection(textEditor);
      hitHomeButton(false, 3, next);
    },

    function testHomeButtonDoesNotChangeCursor(next) {
      var selection = TextUtils.TextRange.createFromLocation(0, 2);
      textEditor.setSelection(selection);
      SourcesTestRunner.dumpTextWithSelection(textEditor);
      hitHomeButton(false, 2, next);
    },

    function testHomeButtonWithShift(next) {
      var selection = new TextUtils.TextRange(0, 0, 2, 8);
      textEditor.setSelection(selection);
      SourcesTestRunner.dumpTextWithSelection(textEditor);
      hitHomeButton(true, 3, next);
    },

    function testHomeButtonWithShiftInversed(next) {
      var selection = new TextUtils.TextRange(3, 1, 2, 8);
      textEditor.setSelection(selection);
      SourcesTestRunner.dumpTextWithSelection(textEditor);
      hitHomeButton(true, 3, next);
    }
  ]);
})();
