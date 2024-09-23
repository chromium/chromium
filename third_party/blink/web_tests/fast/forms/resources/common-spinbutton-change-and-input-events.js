function testSpinButtonChangeAndInputEvents(inputType, writingMode, initialValue, expectedValue, maximumValue)
{
  const description = `Test for event dispatching by spin buttons in a input[type=${inputType}] with writing-mode: ${writingMode}.`;

  test(() => {
    assert_own_property(window, 'eventSender', 'This test requires eventSender.');
    const parent = document.createElement('div');
    document.body.appendChild(parent);
    parent.innerHTML = '<input id=test><input id=another>';
    const testInput = document.getElementById('test');
    const anotherInput = document.getElementById('another');
    let inputEventCounter = 0;
    let changeEventCounter = 0;

    testInput.type = inputType;
    testInput.style.writingMode = writingMode;
    if (maximumValue != undefined)
      testInput.setAttribute("max", maximumValue);
    testInput.setAttribute("value", initialValue);
    testInput.onchange = function() { changeEventCounter++; };
    testInput.oninput = function() { inputEventCounter++; };

    eventSender.mouseMoveTo(0, 0);
    testInput.focus();

    // Move the cursor on the upper button.
    var spinButton = getElementByPseudoId(internals.shadowRoot(testInput), "-webkit-inner-spin-button");
    var rect = spinButton.getBoundingClientRect();
    if (writingMode == 'horizontal-tb' || writingMode == 'sideways-lr') {
      // Near the top left corner.
      eventSender.mouseMoveTo(rect.left + rect.width / 4, rect.top + rect.height / 4);
    } else {
      // Near the top right corner.
      eventSender.mouseMoveTo(rect.left + 3 * rect.width / 4, rect.top + rect.height / 4);
    }
    eventSender.mouseDown();

    let desc = 'Triggers only input event on mouseDown';
    assert_equals(testInput.value, expectedValue, desc);
    assert_equals(changeEventCounter, 0, desc);
    assert_equals(inputEventCounter, 1, desc);

    desc = 'Triggers only change event on mouseUp';
    eventSender.mouseUp();
    assert_equals(testInput.value, expectedValue, desc);
    assert_equals(changeEventCounter, 1, desc);
    assert_equals(inputEventCounter, 1, desc);

    if (testInput.hasAttribute("max")) {
      desc = 'Click again, but the value is not changed.';
      eventSender.mouseDown();
      eventSender.mouseUp();
      assert_equals(testInput.value, expectedValue, desc);
      assert_equals(changeEventCounter, 1, desc);
      assert_equals(inputEventCounter, 1, desc);
    }

    desc = 'Focus on another field';
    anotherInput.focus();
    assert_equals(changeEventCounter, 1, desc);
    assert_equals(inputEventCounter, 1, desc);

    parent.remove();
  }, description);
}
