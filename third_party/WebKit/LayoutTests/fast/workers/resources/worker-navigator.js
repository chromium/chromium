function log(message)
{
    document.getElementById("result").innerHTML += message + "<br>";
}

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

var worker = createWorker();
worker.postMessage("eval typeof navigator");
worker.postMessage("eval navigator");
worker.postMessage("eval navigator.appName");
worker.postMessage("eval navigator.appVersion.indexOf('WebKit') != 0");
worker.postMessage("eval typeof navigator.platform");
worker.postMessage("eval navigator.userAgent.indexOf('WebKit') != 0");
worker.postMessage("eval typeof navigator.onLine");
worker.postMessage("eval navigator.appCodeName === 'Mozilla'");
worker.postMessage("eval navigator.product === 'Gecko'");
worker.postMessage("eval foo//bar");

worker.onmessage = function(evt) {
    if (!/foo\/\/bar/.test(evt.data))
        log(evt.data.replace(new RegExp("/.*(LayoutTests|web_tests)"), "<...>"));
    else {
        log("DONE");
        if (window.testRunner)
            testRunner.notifyDone();
    }
}
