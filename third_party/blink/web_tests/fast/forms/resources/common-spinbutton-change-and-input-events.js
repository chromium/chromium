var inputEventCounter;
var changeEventCounter;
var testInput;

function testSpinButtonChangeAndInputEvents(inputType, writingMode, initialValue, expectedValue, maximumValue)
{
    description(`Test for event dispatching by spin buttons in a input[type=${inputType}] with writing-mode: ${writingMode}.`);
    if (!window.eventSender) {
        debug('No eventSender');
        return;
    }

    inputEventCounter = 0;
    changeEventCounter = 0;

    var parent = document.createElement('div');
    document.body.appendChild(parent);
    parent.innerHTML = '<input id=test><input id=another>';
    testInput = document.getElementById('test');
    var anotherInput = document.getElementById('another');

    testInput.type = inputType;
    testInput.style.writingMode = writingMode;
    if (maximumValue != undefined)
        testInput.setAttribute("max", maximumValue);
    testInput.setAttribute("value", initialValue);
    testInput.onchange = function() { changeEventCounter++; };
    testInput.oninput = function() { inputEventCounter++; };

    debug('Initial state');
    eventSender.mouseMoveTo(0, 0);
    shouldEvaluateTo('changeEventCounter', 0);
    shouldEvaluateTo('inputEventCounter', 0);
    testInput.focus();

    debug('Click the upper button');
    // Move the cursor on the upper button.
    var spinButton = getElementByPseudoId(internals.shadowRoot(testInput), "-webkit-inner-spin-button");
    var rect = spinButton.getBoundingClientRect();
    eventSender.mouseMoveTo(rect.left + rect.width / 4, rect.top + rect.height / 4);
    eventSender.mouseDown();
    debug('Triggers only input event on mouseDown');
    shouldBeEqualToString('testInput.value', expectedValue);
    shouldEvaluateTo('changeEventCounter', 0);
    shouldEvaluateTo('inputEventCounter', 1);
    debug('Triggers only change event on mouseUp');
    eventSender.mouseUp();
    shouldBeEqualToString('testInput.value', expectedValue);
    shouldEvaluateTo('changeEventCounter', 1);
    shouldEvaluateTo('inputEventCounter', 1);

    if (testInput.hasAttribute("max")) {
        debug('Click again, but the value is not changed.');
        eventSender.mouseDown();
        eventSender.mouseUp();
        shouldBeEqualToString('testInput.value', expectedValue);
        shouldEvaluateTo('changeEventCounter', 1);
        shouldEvaluateTo('inputEventCounter', 1);
    }

    debug('Focus on another field');
    anotherInput.focus();
    shouldEvaluateTo('changeEventCounter', 1);
    shouldEvaluateTo('inputEventCounter', 1);

    parent.innerHTML = '';
}
