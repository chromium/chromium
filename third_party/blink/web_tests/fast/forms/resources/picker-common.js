window.jsTestIsAsync = true;

var popupWindow = null;

var popupOpenCallback = null;

function popupOpenCallbackWrapper() {
    popupWindow.removeEventListener("didOpenPicker", popupOpenCallbackWrapper);
    // We need some delay.  Without it, testRunner.notifyDone() freezes.
    // See crbug.com/562311.
    setTimeout(popupOpenCallback, 20);
}

function waitUntilClosing(callback) {
    setTimeout(callback, 1);
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
    popupWindow = openPickerHelper(element);
    if (typeof callback === "function" && popupWindow)
        setPopupOpenCallback(callback);
    else if (typeof errorCallback === "function" && !popupWindow)
        errorCallback();
}

// openPickerAppearanceOnly opens a picker UI for the following types:
// - menulist SELECT
// - INPUT color
// - INPUT date/datetime-local/month/week

// This is intended for use with picker UI tests that are only testing picker
// appearance. Therefore, it is expected that tests using this API should fail
// if the picker does not open.
function openPickerAppearanceOnly(element, callback) {
    let errorCallback = undefined;
    if (typeof testFailed === 'function') {
        errorCallback = () => testFailed('Popup failed to open.');
    }
    openPicker(element, callback, errorCallback);
  }

// openPickerWithPromise opens a picker UI for the following types:
// - menulist SELECT
// - INPUT color
// - INPUT date/datetime-local/month/week
//
// Returns a Promise that resolves when the popup has been opened.
function openPickerWithPromise(element) {
    return new Promise(function(resolve, reject) {
        popupWindow = openPickerHelper(element);
        if (popupWindow) {
            popupOpenCallback = resolve;
            popupWindow.addEventListener("didOpenPicker", popupOpenCallbackWrapper, false);
        } else {
            reject();
        }
    });
}

// Helper function for openPicker and openPickerWithPromise.
// Performs the keystrokes that will cause the picker to open,
// and returns the popup window, or null.
function openPickerHelper(element) {
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
    return internals.pagePopupWindow;
}

// TODO(crbug.com/1047176) - use clickToOpenPickerWithPromise instead
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

// Uses test_driver to open the picker.
function clickToOpenPickerWithPromise(x, y, callback, errorCallback) {
    return new Promise((resolve, reject)=>{
      var actions = new test_driver.Actions();
      actions
          .pointerMove(x, y)
          .pointerDown()
          .pointerUp()
          .send();
      waitUntil(()=>internals.pagePopupWindow).then(()=>{
        popupWindow = internals.pagePopupWindow;
        if (typeof callback === "function")
          setPopupOpenCallback(callback);
        resolve();
      }).catch((err)=>{
          if (typeof errorCallback === "function" && !popupWindow)
            errorCallback();
         reject();
      });
    });
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
