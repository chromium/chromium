function runTest() {
    var clientRect = document.getElementById('targetDiv').getBoundingClientRect();
    x = (clientRect.left + clientRect.right) / 2;
    y = (clientRect.top +  clientRect.bottom) / 2;
    if (window.eventSender) {
        eventSender.gestureShowPress(x, y);
    } else {
        debug("This test requires DumpRenderTree.");
    }
}
