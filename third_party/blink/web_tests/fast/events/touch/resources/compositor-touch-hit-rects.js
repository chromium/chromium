// Helper for running touch-action rect tests and dumping the touch
// action regions from cc::Layers.

// When running these tests manually, the touch action rects can be
// visualized using the inspector: open devtools and press 'esc' to open
// the bottom drawer, then go to the 'Rendering' tab (may need to be
// enabled by clicking the 3-dot menu on the drawer), then check
// "Scrolling performance issues" and the touch-action rects will appear
// in blue.

function listener() {
}

function log(msg) {
    var span = document.createElement("span");
    document.getElementById("console").appendChild(span);
    span.innerHTML = msg + '<br />';
}

function sortRects(a, b) {
    return a.hitTestRect.top - b.hitTestRect.top
        || a.hitTestRect.left - b.hitTestRect.left
        || a.hitTestRect.width - b.hitTestRect.width
        || a.hitTestRect.height - b.hitTestRect.height;
}

var preRunHandlerForTest = {};

function testElement(element) {
    element.addEventListener('touchstart', listener, {passive: false});

    // Run any test-specific handler AFTER adding the touch event listener
    // (which itself causes rects to be recomputed).
    if (element.id in preRunHandlerForTest)
        preRunHandlerForTest[element.id](element);

    if (window.internals)
        internals.forceCompositingUpdate(document);

    logRects(element.id);

    // If we're running manually, leave the handlers in place so the user
    // can use dev tools 'show potential scroll bottlenecks' for visualization.
    if (window.internals)
        element.removeEventListener('touchstart', listener, false);
}

function logRects(testName, opt_noOverlay) {
    if (!window.internals) {
        log(testName + ': not run');
        return;
    }

    var rects = internals.touchEventTargetLayerRects(document);
    if (rects.length == 0)
        log(testName + ': no rects');

    var sortedRects = new Array();
    for ( var i = 0; i < rects.length; ++i)
        sortedRects[i] = rects[i];
    sortedRects.sort(sortRects);
    for ( var i = 0; i < sortedRects.length; ++i) {
        var rect = sortedRects[i].layerRect;
        // Logging width/height for layer identification assistance.
        var rect_string = `${rect.width}x${rect.height}`;
        var hit = sortedRects[i].hitTestRect;
        var hit_string = `${hit.x},${hit.y} ${hit.width}x${hit.height}`;
        log(`${testName}: layer(${rect_string}) has hit test rect (${hit_string})`);
    }

    log('');
}

if (window.testRunner) {
    testRunner.dumpAsText();
    document.documentElement.setAttribute('dumpRenderTree', 'true');
}

window.onload = function() {
    // Run each general test case.
    var tests = document.querySelectorAll('.testcase');

    // Add document wide touchend and touchcancel listeners and ensure the
    // listeners do not affect compositor hit test rects.
    document.documentElement.addEventListener('touchend', listener, false);
    document.documentElement.addEventListener('touchcancel', listener, false);

    for ( var i = 0; i < tests.length; i++) {
        // Force a compositing update before testing each case to ensure that
        // any subsequent touch rect updates are actually done because of
        // the event handler changes in the test itself.
        if (window.internals)
            internals.forceCompositingUpdate(document);
        testElement(tests[i]);
    }

    if (window.additionalTests)
        additionalTests();

    if (window.internals) {
        var testContainer = document.getElementById("tests");
        testContainer.parentNode.removeChild(testContainer);
    }

    document.documentElement.setAttribute('done', 'true');
};
