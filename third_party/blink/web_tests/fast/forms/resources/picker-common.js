window.jsTestIsAsync = true;

var popupWindow = null;

var popupOpenCallback = null;

function popupOpenCallbackWrapper() {
    popupWindow.removeEventListener("didOpenPicker", popupOpenCallbackWrapper);
    // We need some delay.  Without it, testRunner.notifyDone() freezes.
    // See crbug.com/562311.
    setTimeout(popupOpenCallback, 20);
}

function waitUntilClosing(callback, customDelay) {
    setTimeout(callback, (customDelay !== undefined) ? customDelay : 1);
}

function rootWindow() {
    var currentWindow = window;
    while (currentWindow !== currentWindow.parent) {
        currentWindow = currentWindow.parent;
    }
    return currentWindow;
}

// openPicker opens a picker UI for the following types:
// - menulist SELECT
// - INPUT color
// - INPUT date/datetime-local/month/week
//
// |callback| is called if we successfully open the picker UI. However it is
// called only for the following types:
// - menulist SELECT on Windows, Linux, and CrOS
// - INPUT color with DATALIST
// - INPUT date/datetime-local/month/week
function openPicker(element, callback, errorCallback) {
    element.offsetTop; // Force to lay out
    element.focus();
    if (element.tagName === "SELECT") {
        eventSender.keyDown("ArrowDown", ["altKey"]);
    } else if (element.tagName === "INPUT") {
        if (element.type === "color") {
            eventSender.keyDown(" ");
        } else {
            eventSender.keyDown("ArrowDown", ["altKey"]);
        }
    }
    popupWindow = internals.pagePopupWindow;
    if (typeof callback === "function" && popupWindow)
        setPopupOpenCallback(callback);
    else if (typeof errorCallback === "function" && !popupWindow)
        errorCallback();
}

function clickToOpenPicker(x, y, callback, errorCallback) {
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();
    popupWindow = internals.pagePopupWindow;
    if (typeof callback === "function" && popupWindow)
        setPopupOpenCallback(callback);
    else if (typeof errorCallback === "function" && !popupWindow)
        errorCallback();
}

function setPopupOpenCallback(callback) {
    console.assert(popupWindow);
    popupOpenCallback = callback;
    try {
        popupWindow.addEventListener("didOpenPicker", popupOpenCallbackWrapper, false);
    } catch(e) {
        debug(e.name);
    }
}
