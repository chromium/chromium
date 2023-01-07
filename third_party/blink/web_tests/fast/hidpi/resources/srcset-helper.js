function runTest() {
    if (!window.testRunner || !window.sessionStorage)
        return;

    if (!window.targetScaleFactor)
        window.targetScaleFactor = 2;

    if (!sessionStorage.scaleFactorIsSet) {
        testRunner.waitUntilDone();
        testRunner.setBackingScaleFactor(targetScaleFactor, scaleFactorIsSet);
    }

    if (sessionStorage.pageReloaded && sessionStorage.scaleFactorIsSet) {
        delete sessionStorage.pageReloaded;
        delete sessionStorage.scaleFactorIsSet;
        if (!window.manualNotifyDone) {
            setTimeout(function() {
                testRunner.notifyDone();
            }, 0);
        }
    } else {
        // Right now there is a bug that srcset does not properly deal with dynamic changes to the scale factor,
        // so to work around that, we must reload the page to get the new image.
        sessionStorage.pageReloaded = true;
        document.location.reload(true);
    }
}

function scaleFactorIsSet() {
    sessionStorage.scaleFactorIsSet = true;
}

window.addEventListener("load", runTest, false);
