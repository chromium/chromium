function log(msg) {
    document.getElementById('console').appendChild(document.createTextNode(msg + '\n'));
}

if (window.testRunner && window.eventSender) {
    testRunner.setTextSubpixelPositioning(true);
    testRunner.dumpAsText();

    var x = floatedEditable.offsetLeft + (floatedEditable.offsetWidth / 2);
    var y = floatedEditable.offsetTop + (floatedEditable.offsetHeight / 2);
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    x = floatedEditable.offsetLeft + floatedEditable.offsetWidth + 5;
    eventSender.mouseMoveTo(x, y);
    eventSender.leapForward(250);
    eventSender.mouseUp();

    checkSelection();
} else {
    window.onmouseup = function() {
        window.setTimeout(function() {
            log('Input selection start: ' + getSelectionStart(floatedEditable) + ', end: ' +
                getSelectionEnd(floatedEditable));
            checkSelection();
        }, 0);  // Without a timeout the selection is inaccurately printed
    }
}

function getSelectionStart(element) {
    return element.isContentEditable ? window.getSelection().baseOffset : element.selectionStart;
}

function getSelectionEnd(element) {
    return element.isContentEditable ? window.getSelection().extentOffset : element.selectionEnd;
}

function checkSelection() {
    var inputText = floatedEditable.isContentEditable ? floatedEditable.textContent : floatedEditable.value;
    var selectionStart = getSelectionStart(floatedEditable);
    var selectionEnd = getSelectionEnd(floatedEditable);

    var selectionStartsFromMiddle = selectionStart > 0 && selectionStart < inputText.length;
    var selectionGoesToEnd = selectionEnd == inputText.length;
    if (selectionStartsFromMiddle && selectionGoesToEnd)
        result.innerHTML = '<span style="padding: 5px; background-color: green">SUCCESS</span>';
    else
        result.innerHTML = '<span style="padding: 5px; background-color: red">FAIL</span>';
}
