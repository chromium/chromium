// Wait until any mouse cursor update has completed, then verify the cursor info
// is what's expected and call the provided continuation.
// We need to poll to ensure EventHandler's cursor update timer has had a chance to fire.
function expectCursorUpdate(expectedInfo, completion) {
    // Need to give style application a chance to take effect first.
    requestAnimationFrame(function() {
        // Note that cursorUpdatePending should (almost?) always be true at this
        // point, but we probably shouldn't depend on that in case scheduler changes
        // result in rAF not firing until after the cursor update timer as already
        // fired.
        var onFrame = function() {
            if (internals.cursorUpdatePending) {
                requestAnimationFrame(onFrame);
            } else {
                shouldBeEqualToString('internals.getCurrentCursorInfo()', expectedInfo);
                completion();
            }
        }
        requestAnimationFrame(onFrame);
    });
}
