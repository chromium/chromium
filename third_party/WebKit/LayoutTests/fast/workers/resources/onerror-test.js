function stripURL(url) {
    if (url.match(/^blob:/))
        return "[blob: URL]";
    return url ? url.match( /[^\/]+\/?$/ )[0] : url;
}

function checkWorkerLevelErrorEvent(errorEvent, obj) {
    window.errorEvent = errorEvent;
    shouldBeEqualToString('errorEvent.message', obj.message);
    shouldBeEqualToString('stripURL(errorEvent.filename)', obj.filename);
    shouldBe('errorEvent.lineno', '' + obj.lineno);
    shouldBe('errorEvent.colno', '' + obj.colno);
}

function checkPageLevelErrorEvent(errorEvent, obj) {
    window.errorEvent = errorEvent;
    shouldBeEqualToString('errorEvent.message', obj.message);
    shouldBeEqualToString('stripURL(errorEvent.filename)', obj.filename);
    shouldBe('errorEvent.lineno', '' + obj.lineno);
    shouldBe('errorEvent.colno', '' + obj.colno);

    // The spec says the error property for the errors generated for dedicated
    // workers (not DedicatedWorkerGlobalScope) should be initialized to null.
    // This is because error property cannot always be passed to outside
    // workers, since the value of |error| attribute is not always clonable.
    // https://html.spec.whatwg.org/multipage/workers.html#runtime-script-errors-2
    shouldBeNull('errorEvent.error');
}

function checkPostMessage(obj) {
    window.postMessageCallback = function (e) {
        if (e.data.done)
            return;
        debug("\nWorker-level onerror handler triggered:");
        checkWorkerLevelErrorEvent(e.data, obj);
    };
}

var errorsSeen = 0;
function checkErrorEventInHandler(error, returnValue) {
    var worker = buildInlineWorker();
    worker.onerror = function (e) {
        debug("\nPage-level worker.onerror handler triggered:");
        window.errorEvent = e;
        var obj = error;
        if (error.length)
            obj = error[errorsSeen++];

        checkPageLevelErrorEvent(e, obj);

        if (!error.length || error.length == errorsSeen)
            finishJSTest();

        return returnValue;
    };
}

function checkErrorEventInListener(error, preventDefault) {
    var worker = buildInlineWorker();
    worker.addEventListener('error', function (e) {
        debug("\nPage-level worker 'error' event listener triggered:");
        var obj = error;
        if (error.length)
            obj = error[errorsSeen++];

        checkPageLevelErrorEvent(e, obj);

        if (!error.length || error.length == errorsSeen)
            finishJSTest();

        if (preventDefault)
            e.preventDefault();
    });
}

function errorEventHandlerShouldNotFire() {
    var worker = buildInlineWorker();
    worker.onerror = function (e) {
        testFailed("'worker.onerror' should not have been called.");
    };
}

function buildInlineWorker() {
    var script = document.getElementById('workerCode').innerText;
    var blob = new Blob([script], {type: 'text/javascript'});
    var worker = new Worker(URL.createObjectURL(blob));

    if (window.postMessageCallback)
        worker.addEventListener('message', postMessageCallback);
    worker.addEventListener('message', function (e) {
        if (e.data.done)
            return finishJSTest();
    });

    return worker;
}

window.jsTestIsAsync = true;
