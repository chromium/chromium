// src: local() fonts may resolve asynchronously, see https://crbug.com/939823
if (window.testRunner) {
    testRunner.waitUntilDone();
    document.fonts.ready.then(() => { testRunner.notifyDone(); });
}
