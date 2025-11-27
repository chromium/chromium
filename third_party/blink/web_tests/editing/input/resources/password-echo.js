var testnode;

function secureChar()
{
    var element = testnode;
    var securechar = document.defaultView.getComputedStyle(element, "").getPropertyValue("-webkit-text-security");
    switch(securechar) {
    case "square":
        return String.fromCharCode(0x25A0);
    case "disc":
        return String.fromCharCode(0x2022);
    case "circle":
        return String.fromCharCode(0x25E6);
    }
}

function secureText(textLength)
{
    var text = "";
    for (var counter = 0; counter < textLength; counter++)
        text += secureChar();
    return text;
}

function log(msg)
{
    var console = document.getElementById("console");
    var li = document.createElement("li");
    li.appendChild(document.createTextNode(msg));
    console.appendChild(li);
}

function assert(expected, actual, msg)
{
    if (expected != actual)
        log("Error: " + msg + " expected=" + expected + ", actual=" + actual);
    else
        log("Success: " + msg + " expected=" + expected + ", actual=" + actual);
}

function run(tests, testIdx)
{
    var expectedSecureTextLen;
    if (testIdx >= 0) {
        eventSender.keyDown("ArrowRight");
        if(tests[testIdx][3])
            assert(tests[testIdx][2], window.find(secureText(testnode.value.length), false, true), "secured after delay.");
    }
    testIdx++;
    if (testIdx >= tests.length) {
        testRunner.notifyDone();
        return;
    }

    testnode.focus();
    eventSender.keyDown("ArrowRight");

    var charSequence = tests[testIdx][0];
    for (var i = 0; i < charSequence.length - 1; i++) {
        textInputController.setMarkedText(charSequence[i], testnode.value.length, testnode.value.length);
    }
    if (charSequence[charSequence.length - 1] == "backspace") {
        eventSender.keyDown("ArrowLeft");
        eventSender.keyDown("Delete");
    } else
        textInputController.insertText(charSequence[charSequence.length - 1]);

    if(tests[testIdx][3])
        assert(tests[testIdx][1], window.find(secureText(testnode.value.length), false, true), "secured right after.");

    if(tests[testIdx][3])
        window.setTimeout(function(){ run(tests, testIdx); }, 600);
    else
        window.setTimeout(function(){ run(tests, testIdx); }, 0);
}

function setTestNode(testNode)
{
  testnode = testNode;
}

function init(tests)
{
    if (window.testRunner && window.textInputController && window.eventSender) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
        if (window.internals) {
            internals.settings.setPasswordEchoEnabledPhysical(true);
            internals.settings.setPasswordEchoEnabledTouch(true);
            internals.settings.setPasswordEchoDurationInSeconds(0.1);
            testnode = document.getElementById('testnode');
            run(tests, -1);
        }
    }
}

