if (window.testRunner)
    testRunner.waitUntilDone(), testRunner.dumpAsText();

function checkNoFocusRing(element, event)
{
    var color = getComputedStyle(element, null).getPropertyValue('outline-color');
    var style = getComputedStyle(element, null).getPropertyValue('outline-style');
    var width = getComputedStyle(element, null).getPropertyValue('outline-width');

    var noFocusRing = (width == '0px') || (style == 'none') || (style == 'hidden');

    document.body.insertAdjacentHTML('beforeEnd', '<BR>' + element.tagName +
        ' Event: ' +  event.type);
    document.body.insertAdjacentHTML('beforeEnd', noFocusRing ?
        ' PASS' : ' FAIL: focus style ' + [width, style, color].join(' '));

    if (window.testRunner)
        testRunner.notifyDone();
}

var element = document.getElementById('test');
element.onfocus = function() { setTimeout(checkNoFocusRing, 50, element, event) };

if (window.testRunner) {
    eventSender.mouseMoveTo(element.offsetLeft + 5, element.offsetTop + 5);
    eventSender.mouseDown();
    eventSender.mouseUp();
}
