var testInput;
var blurCounter = 0;
var focusCounter = 0;

function keyDown(key, modifiers)
{
    eventSender.keyDown(key, modifiers);
}

function state()
{
    return 'blur=' + blurCounter + ' focus=' + focusCounter;
}

function beginTestCase(testCaseName)
{
    debug(testCaseName);
    blurCounter = 0;
    focusCounter = 0;
}

function startTestFor(typeName)
{
    description('Check blur and focus events for multiple fields ' + typeName + ' input UI');
    document.getElementById('container').innerHTML = '<input id="before">'
        + '<input id="test" type="' + typeName + '">'
        + '<input id="after">';

    testInput = document.getElementById('test');
    testInput.addEventListener('blur', function () { ++blurCounter; });
    testInput.addEventListener('focus', function () { ++focusCounter; });

    beginTestCase('focus() and blur()');
    shouldBeEqualToString('testInput.focus(); state()', 'blur=0 focus=1');
    shouldBeEqualToString('testInput.blur(); state()', 'blur=1 focus=1');

    if (window.eventSender) {
        var numberOfFields;
        switch (typeName) {
        case 'week':
        case 'month':
            numberOfFields = 2;
            break;
        case 'datetime':
        case 'datetime-local':
            numberOfFields = 6;
            break;
        default:
            numberOfFields = 3;
            break;
        }

        beginTestCase('focus and Tab key to blur');
        document.getElementById("before").focus();
        for (var i = 0; i < numberOfFields; i++)
            shouldBeEqualToString('keyDown("\t"); state()', 'blur=0 focus=1');
        shouldBeEqualToString('keyDown("\t"); state()', 'blur=1 focus=1');
    } else {
        debug('Please run in DumpRenderTree for focus and Tab-key test case');
    }
    debug('');
    document.body.removeChild(document.getElementById("container"));
}
