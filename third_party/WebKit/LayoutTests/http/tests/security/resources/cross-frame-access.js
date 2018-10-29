function log(s)
{
    document.getElementById("console").appendChild(document.createTextNode(s + "\n"));
}

function shouldBe(a, b, shouldNotPrintValues)
{
    var evalA, evalB;
    try {
        evalA = eval(a);
    } catch(e) {
        evalA = e.toString();
    }

    try {
        evalB = eval(b);
    } catch(e) {
        evalB = e.toString();
    }

    var message;
    if (evalA === evalB) {
        message = "PASS";
        if (!shouldNotPrintValues) {
            message += ": " + a + " should be '" + evalB + "' and is.";
        } else {
            message += ": " + a + " matched the expected value.";
        }
    } else {
       message = "*** FAIL: " + a + " should be '" + evalB + "' but instead is " + evalA + ". ***";
    }

    message = String(message).replace(/\n/g, "");
    log(message);
}

function shouldBeTrue(a)
{
    shouldBe(a, "true");
}

function shouldBeFalse(b)
{
    shouldBe(b, "false");
}

function shouldBeUndefined(b)
{
    shouldBe(b, "undefined");
}

function canGet(keyPath)
{
    try {
        return eval("window." + keyPath) !== undefined;
    } catch(e) {
        return false;
    }
}

function shouldThrowException(a) {
    try {
        var evalA = eval(a);
        log("FAIL: " + a + " should have thrown an exception, but instead returned " + evalA + ".");
        return false;
    } catch (e) {
        log("PASS: " + a + " should have thrown an exception, and did: " + e.toString());
        return true;
    }
}

function accessThrowsException(keyPath) {
    try {
        eval("window." + keyPath);
        return false;
    } catch (e) {
        return true;
    }
}

function deletionThrowsException(keyPath) {
    try {
        eval("delete " + keyPath);
        return false;
    } catch (e) {
        return true;
    }
}

function canGetDescriptor(target, property)
{
    try {
        var desc = Object.getOwnPropertyDescriptor(target, property);
        // To deal with an idiosyncrasy in how binding objects work in conjunction with
        // slot and descriptor delegates we also allow descriptor with undefined value/getter/setter
        return desc !== undefined && (desc.value !== undefined || desc.get !== undefined || desc.set !== undefined);
    } catch(e) {
        return false;
    }
}

window.marker = { };

function canSet(keyPath, valuePath)
{
    if (valuePath === undefined)
        valuePath = "window.marker";

    try {
        eval("window." + keyPath + " = " + valuePath);
        return eval("window." + keyPath) === eval("window." + valuePath);
    } catch(e) {
        return false;
    }
}

function canCall(keyPath, argumentString)
{
    try {
        eval("window." + keyPath + "(" + (argumentString === undefined ? "" : "'" + argumentString + "'") + ")");
        return true;
    } catch(e) {
        return false;
    }
}

function toString(expression, valueForException)
{
    if (valueForException === undefined)
        valueForException = "[exception]";

    try {
        var evalExpression = eval(expression);
        if (evalExpression === undefined)
            throw null;
        return String(evalExpression);
    } catch(e) {
        return valueForException;
    }
}

// Frame Access Tests

function canAccessFrame(iframeURL, iframeId, passMessage, failMessage)
{
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.dumpChildFrames();
        testRunner.waitUntilDone();
    }

    window.addEventListener("message", function(event) {
        if (event.data == "LOADED") {
            test();
        }
    }, false);

    var targetWindow = frames[0];
    if (!targetWindow.document.body)
        log("FAIL: targetWindow started with no document, we won't know if the test passed or failed.");

    var iframe = document.getElementById(iframeId);
    iframe.src = iframeURL;

    function test()
    {
        try {
            if (targetWindow.document && targetWindow.document.getElementById('accessMe')) {
                targetWindow.document.getElementById('accessMe').innerHTML = passMessage;
                log(passMessage);
                if (window.testRunner)
                    testRunner.notifyDone();
                return;
            }
        } catch (e) {
            log("In catch");
        }

        log(failMessage);
        if (window.testRunner)
            testRunner.notifyDone();
    }
}

function cannotAccessFrame(iframeURL, iframeId, passMessage, failMessage)
{
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.dumpChildFrames();
        testRunner.waitUntilDone();
    }

    window.addEventListener("message", function(event) {
        if (event.data == "LOADED") {
            test();
        }
    }, false);

    var targetWindow = frames[0];
    if (!targetWindow.document.body)
        log("FAIL: targetWindow started with no document, we won't know if the test passed or failed.");

    var iframe = document.getElementById(iframeId);
    iframe.src = iframeURL;

    function test()
    {
        try {
            if (targetWindow.document && targetWindow.document.getElementById('accessMe')) {
                targetWindow.document.getElementById('accessMe').innerHTML = failMessage;
                log(failMessage);
                if (window.testRunner)
                    testRunner.notifyDone();
                return;
            }
        } catch (e) {
        }

        log(passMessage);
        if (window.testRunner)
            testRunner.notifyDone();
    }
}

function closeWindowAndNotifyDone(win)
{
    win.close();
    setTimeout(doneHandler, 5);
    function doneHandler() {
        if (win.closed) {
            if (window.testRunner)
                testRunner.notifyDone();
            return;
        }

        setTimeout(doneHandler, 5);
    }
}
