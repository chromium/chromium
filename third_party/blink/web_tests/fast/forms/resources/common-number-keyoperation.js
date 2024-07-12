var input;

function testNumberKeyoperations(writingMode, initialValue) {
    debug(`\nTest starts for &lt;input type=number style="writing-mode: ${writingMode}">`);
    if (!window.eventSender)
        return;
    let stepDownKey;
    let stepUpKey;
    let moveToNextDigit;
    let moveToPrevDigit;
    if (writingMode == "horizontal-tb") {
      stepDownKey = 'ArrowDown';
      stepUpKey = 'ArrowUp';
      moveToNextDigit = 'ArrowRight';
      moveToPrevDigit = 'ArrowLeft';
    } else if (writingMode == 'sideways-lr') {
      stepDownKey = 'ArrowRight';
      stepUpKey = 'ArrowLeft';
      moveToNextDigit = 'ArrowUp';
      moveToPrevDigit = 'ArrowDown';
    } else {
      stepDownKey = 'ArrowLeft';
      stepUpKey = 'ArrowRight';
      moveToNextDigit = 'ArrowDown';
      moveToPrevDigit = 'ArrowUp';
    }
    var parent = document.createElement('div');
    document.body.appendChild(parent);
    parent.innerHTML = '<input type=number id=number>';

    input = document.getElementById('number');
    input.style.writingMode = writingMode;
    input.focus();
    debug('Inserting "ab123cd":');
    document.execCommand('InsertText', false, `ab${initialValue}cd`);
    shouldBeEqualToString('input.value', `${initialValue}`);

    debug('Press the up arrow key:');
    input.valueAsNumber = initialValue;
    eventSender.keyDown(stepUpKey);
    shouldBeEqualToString('input.value', `${initialValue + 1}`);

    debug('Press the down arrow key:');
    eventSender.keyDown(stepDownKey);
    shouldBeEqualToString('input.value', `${initialValue}`);

    debug('Press the down and alt arrow key, should not decrement value:');
    eventSender.keyDown(stepDownKey, ['altKey']);
    shouldBeEqualToString('input.value', `${initialValue}`);

    debug('Disable input element:');
    input.disabled = true;
    eventSender.keyDown(stepUpKey);
    shouldBeEqualToString('input.value', `${initialValue}`);
    input.removeAttribute('disabled');

    debug('Read-only input element:');
    input.readonly = true;
    eventSender.keyDown(stepUpKey);
    shouldBeEqualToString('input.value', `${initialValue}`);
    input.removeAttribute('readonly');

    debug('Moving caret by pressing arrow keys:');
    input.focus();
    eventSender.keyDown(moveToNextDigit);
    shouldBeEqualToString('input.value', `${initialValue}`);
    eventSender.keyDown(moveToPrevDigit);
    shouldBeEqualToString('input.value', `${initialValue}`);

    parent.innerHTML = '';
}