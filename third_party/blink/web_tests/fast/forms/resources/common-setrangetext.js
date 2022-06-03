// TODO(xiaochengh): Convert all setRangeText tests to use testharness.

function runTestsShouldPass(tagName, attributes)
{
    attributes = attributes || {};
    window.element = document.createElement(tagName);
    for (var key in attributes)
        element.setAttribute(key, attributes[key]);
    document.body.appendChild(element);
    debug("<hr>");
    debug("Running tests on " + tagName + " with attributes: " + JSON.stringify(attributes) + "\n");
    debug("setRangeText() with only one parameter.");

    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(2, 5)");
    evalAndLog("element.setRangeText('ABC')");
    shouldBeEqualToString("element.value", "01ABC56789");
    shouldBe("element.selectionStart", "2");
    shouldBe("element.selectionEnd", "5");
    evalAndLog("element.setRangeText('ABCD')");
    shouldBeEqualToString("element.value", "01ABCD56789");
    shouldBe("element.selectionStart", "2");
    shouldBe("element.selectionEnd", "6");
    evalAndLog("element.setRangeText('AB')");
    shouldBeEqualToString("element.value", "01AB56789");
    shouldBe("element.selectionStart", "2");
    shouldBe("element.selectionEnd", "4");
    evalAndLog("element.setRangeText('')");
    shouldBeEqualToString("element.value", "0156789");
    shouldBe("element.selectionStart", "2");
    shouldBe("element.selectionEnd", "2");

    debug("\nsetRangeText() with 'select' as the selectMode.");
    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(0, 0)");
    evalAndLog("element.setRangeText('ABC', 2, 5, 'select')");
    shouldBeEqualToString("element.value", "01ABC56789");
    shouldBe("element.selectionStart", "2");
    shouldBe("element.selectionEnd", "5");

    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(0, 0)");
    evalAndLog("element.setRangeText('ABC', 5, 10, 'select')");
    shouldBeEqualToString("element.value", "01234ABC");
    shouldBe("element.selectionStart", "5");
    shouldBe("element.selectionEnd", "8");

    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(0, 0)");
    evalAndLog("element.setRangeText('ABC', 1, 2, 'select')");
    shouldBeEqualToString("element.value", "0ABC23456789");
    shouldBe("element.selectionStart", "1");
    shouldBe("element.selectionEnd", "4");

    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(0, 0)");
    evalAndLog("element.setRangeText('', 1, 9, 'select')");
    shouldBeEqualToString("element.value", "09");
    shouldBe("element.selectionStart", "1");
    shouldBe("element.selectionEnd", "1");

    debug("\nsetRangeText() with 'start' as the selectMode.");
    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(0, 0)");
    evalAndLog("element.setRangeText('ABC', 2, 6, 'start')");
    shouldBeEqualToString("element.value", "01ABC6789");
    shouldBe("element.selectionStart", "2");
    shouldBe("element.selectionEnd", "2");

    debug("\nsetRangeText() with 'end' as the selectMode.");
    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(0, 0)");
    evalAndLog("element.setRangeText('ABC', 10, 10, 'end')");
    shouldBeEqualToString("element.value", "0123456789ABC");
    shouldBe("element.selectionStart", "13");
    shouldBe("element.selectionEnd", "13");

    debug("\nsetRangeText() with 'preserve' as the selectMode.");
    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(6, 9)");
    evalAndLog("element.setRangeText('A', 1, 2)"); // selectMode is optional and defaults to preserve.
    shouldBeEqualToString("element.value", "0A23456789");
    shouldBe("element.selectionStart", "6");
    shouldBe("element.selectionEnd", "9");

    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(6, 9)");
    shouldThrow("element.setRangeText('AB', 1, 1, 'invalid')"); // Invalid selectMode should throw TypeError
    shouldBeEqualToString("element.value", "0123456789");
    shouldBe("element.selectionStart", "6");
    shouldBe("element.selectionEnd", "9");

    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(6, 9)");
    evalAndLog("element.setRangeText('AB', 1, 1, undefined)"); // Undefined selectMode also default to preserve.
    shouldBeEqualToString("element.value", "0AB123456789");
    shouldBe("element.selectionStart", "8");
    shouldBe("element.selectionEnd", "11");

    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(6, 9)");
    evalAndLog("element.setRangeText('A', 1, 3, 'preserve')");
    shouldBeEqualToString("element.value", "0A3456789");
    shouldBe("element.selectionStart", "5");
    shouldBe("element.selectionEnd", "8");

    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(2, 6)");
    evalAndLog("element.setRangeText('A', 1, 4, 'preserve')");
    shouldBeEqualToString("element.value", "0A456789");
    shouldBe("element.selectionStart", "1");
    shouldBe("element.selectionEnd", "4");

    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(2, 6)");
    evalAndLog("element.setRangeText('A', 4, 6, 'preserve')");
    shouldBeEqualToString("element.value", "0123A6789");
    shouldBe("element.selectionStart", "2");
    shouldBe("element.selectionEnd", "5");

    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(2, 6)");
    evalAndLog("element.setRangeText('ABCDEF', 4, 7, 'preserve')");
    shouldBeEqualToString("element.value", "0123ABCDEF789");
    shouldBe("element.selectionStart", "2");
    shouldBe("element.selectionEnd", "10");

    debug("\nsetRangeText() with various start/end values.");
    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(0, 0)");
    evalAndLog("element.setRangeText('A', 100, 100, 'select')");
    shouldBeEqualToString("element.value", "0123456789A");
    shouldBe("element.selectionStart", "10");
    shouldBe("element.selectionEnd", "11");

    evalAndLog("element.value = '0123456789'");
    evalAndLog("element.setSelectionRange(0, 0)");
    evalAndLog("element.setRangeText('A', 8, 100, 'select')");
    shouldBeEqualToString("element.value", "01234567A");
    shouldBe("element.selectionStart", "8");
    shouldBe("element.selectionEnd", "9");

    evalAndLog("element.value = '0123456789'");
    shouldThrow("element.setRangeText('A', 7, 3)");
}

function runTestsShouldFail(tagName, attributes)
{
    attributes = attributes || {};
    window.element = document.createElement(tagName);
    for (var key in attributes)
        element.setAttribute(key, attributes[key]);

    document.body.appendChild(element);
    debug("<hr>");
    debug("Running tests on " + tagName + " with attributes: " + JSON.stringify(attributes) + "\n");
    if (element.getAttribute("type") == "file")
        shouldThrow("element.value = '0123456789XYZ'");
    else
        evalAndLog("element.value = '0123456789XYZ'");
    var initialValue = element.value;
    shouldThrow("element.setRangeText('ABC', 0, 0)");
    // setRangeText() shouldn't do anything on non-text form controls.
    shouldBeEqualToString("element.value", initialValue);
}

function runTestsShouldFailTestHarness(tagName, attributes, descriptions, title) {
  attributes = attributes || {};
  window.element = document.createElement(tagName);
  for (var key in attributes)
    element.setAttribute(key, attributes[key]);

  document.body.appendChild(element);
  test(() => {
    if (element.getAttribute("type") == "file") {
      assert_throws_dom('InvalidStateError', () => element.value = '0123456789XYZ', descriptions.shift());
    } else {
      element.value = '0123456789XYZ';
    }

    var initialValue = element.value;
    assert_throws_dom('InvalidStateError', () => element.setRangeText('ABC', 0, 0), descriptions.shift());

    // setRangeText() shouldn't do anything on non-text form controls.
    assert_equals(element.value, initialValue);
  }, title);
}

