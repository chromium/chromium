jsTestIsAsync = true;

// Positive deltaX or deltaY means scroll right or down.
async function dispatchWheelEvent(element, deltaX, deltaY)
{
    element_center = elementCenter(element);
    await wheelTick(deltaX, deltaY, element_center);
    await waitForCompositorCommit();
}

var input;
async function testWheelEvent(parameters)
{
    var inputType = parameters['inputType'];
    var initialValue = parameters['initialValue'];
    var stepUpValue1 = parameters['stepUpValue1'];
    var stepUpValue2 = parameters['stepUpValue2'];
    description('Test for wheel operations for &lt;input type=' + inputType + '>');
    var parent = document.createElement('div');
    document.body.appendChild(parent);
    parent.innerHTML = '<input type=' + inputType + ' id=test value="' + initialValue + '"> <input id=another>';
    input = document.getElementById('test');
    input.focus();

    debug('Initial value is ' + initialValue + '. We\'ll wheel up by 1:');
    await dispatchWheelEvent(input, 0, -1);
    shouldBeEqualToString('input.value', stepUpValue1);

    // We change the selected value in ScrollBegin and the three wheel ticks
    // are treated as a single stream with one ScrollBegin, so we increase or
    // decrease by one value even though there is more than one wheel tick.
    debug('Wheel up by 3:');
    await dispatchWheelEvent(input, 0, -3);
    shouldBeEqualToString('input.value', stepUpValue2);

    debug('Wheel down by 1:');
    await dispatchWheelEvent(input, 0, 1);
    shouldBeEqualToString('input.value', stepUpValue1);

    debug('Wheel down by 3:');
    await dispatchWheelEvent(input, 0, 3);
    shouldBeEqualToString('input.value', initialValue);

    debug('Disabled input element:');
    input.disabled = true;
    await dispatchWheelEvent(input, 0, -1);
    shouldBeEqualToString('input.value', initialValue);
    input.removeAttribute('disabled');


    debug('Read-only input element:');
    input.readOnly = true;
    await dispatchWheelEvent(input, 0, -1);
    shouldBeEqualToString('input.value', initialValue);
    input.readOnly = false;

    debug('No focus:');
    document.getElementById('another').focus();
    await dispatchWheelEvent(input, 0, -1);
    shouldBeEqualToString('input.value', initialValue);

    parent.parentNode.removeChild(parent);
}
