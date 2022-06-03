//
// shadow-test-driver.js
//
// To use shadow-test-driver.js, you should have
//   <div id="actual-container"></div>
//   <div id="expect-container"></div>
//   <pre id="console"></pre>
// in your body.
//
// Then, define test functions having one argument 'callIfDone'.
// callIfDone should be called when your test function finished.
//
// In body.onload, call doTest(testFuncs) where testFuncs is an array of test functions.
//
// See content-element-move.html as an example.
//

function log(message) {
    document.getElementById('console').innerHTML += (message + "\n");
}

function removeAllChildren(elem) {
    while (elem.firstChild)
        elem.removeChild(elem.firstChild);
}

function cleanUp() {
    removeAllChildren(document.getElementById('actual-container'));
    removeAllChildren(document.getElementById('expect-container'));
}

function removeContainerLines(text) {
    var lines = text.split('\n');
    lines.splice(0, 2);
    return lines.join('\n');
}

function check() {
    var expectContainer = document.getElementById('expect-container');
    var actualContainer = document.getElementById('actual-container');
    var originalDisplayValue = actualContainer.style.display;
    actualContainer.style.display = 'none';
    expectContainer.offsetLeft;
    var refContainerLayoutTree = internals.elementLayoutTreeAsText(expectContainer);
    var refLayoutTree = removeContainerLines(refContainerLayoutTree);
    actualContainer.style.display = originalDisplayValue;

    originalDisplayValue = expectContainer.style.display;
    expectContainer.style.display = 'none';
    actualContainer.offsetLeft;
    var targetContainerLayoutTree = internals.elementLayoutTreeAsText(actualContainer);
    var targetLayoutTree = removeContainerLines(targetContainerLayoutTree);
    expectContainer.style.display = originalDisplayValue;

    if (targetLayoutTree == refLayoutTree)
        log("PASS");
    else {
        log("FAIL");
        log("Expected: ");
        log(refLayoutTree);
        log("Actual: ");
        log(targetLayoutTree);
    }
}

function createSpanWithText(text, className) {
    var span = document.createElement('span');
    span.appendChild(document.createTextNode(text));
    if (className)
        span.className = className;
    return span;
}

function createContentWithSelect(select, fallbackText) {
    var content = document.createElement('content');
    content.setAttribute('select', select);
    if (fallbackText)
        content.appendChild(createSpanWithText(fallbackText));

    return content;
}

function createContentWithText(fallbackText) {
    var content = document.createElement('content');
    if (fallbackText)
        content.innerHTML = fallbackText;

    return content;
}

function appendShadow(target, select) {
    var root = target.attachShadow({mode: 'open'});

    var content = document.createElement('content');
    content.setAttribute('select', select);
    content.appendChild(createSpanWithText("FALLBACK"));

    root.appendChild(document.createTextNode("{SHADOW: "));
    root.appendChild(content);
    root.appendChild(document.createTextNode("}"));
}

function appendShadowDeep(target, select) {
    var root = target.attachShadow({mode: 'open'});

    var child = document.createElement("span");
    {
        var content = document.createElement('content');
        content.setAttribute('select', select);
        content.appendChild(createSpanWithText("FALLBACK"));

        child.appendChild(document.createTextNode("{INNER: "));
        child.appendChild(content);
        child.appendChild(document.createTextNode("}"));
    }

    root.appendChild(document.createTextNode("{SHADOW: "));
    root.appendChild(child);
    root.appendChild(document.createTextNode("}"));
}

function doTestIfLeft(restTests) {
    var test = restTests.shift();
    if (test == null)
        return doneTest();

    var callIfDone = function() {
        setTimeout(function() {
            check();
            cleanUp();
            doTestIfLeft(restTests);
        }, 0);
    };

    log(test.name);
    test(callIfDone);
}

function doneTest() {
    log("TEST COMPLETED");
    if (window.tearDownOnce)
        window.tearDownOnce();
    testRunner.notifyDone();
}

// A test driver. Call this body.onload.
function doTest(tests) {
    if (window.setUpOnce)
        window.setUpOnce();

    if (window.testRunner) {
        testRunner.waitUntilDone();
        testRunner.dumpAsText();
    }

    cleanUp();
    doTestIfLeft(tests);
}
