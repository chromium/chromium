function requestAnimationFramePromise(w) {
  return new Promise((resolve) => (w || window).requestAnimationFrame(resolve));
}

function waitUntilOpen(popupWindow) {
  return new Promise((resolve, reject) => {
    if (!popupWindow)
      reject('popupWindow is null');
    function tick() {
      // didOpenPicker gets set by pickerCommon.js: https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/html/forms/resources/pickerCommon.js;l=213;drc=f2f1e770d7def6c722b0092d77b2d3395c46a477
      if (popupWindow.didOpenPicker)
        resolve();
      else
        requestAnimationFrame(tick.bind(this));
    }
    tick();
  });
}

// Returns a promise that resolves when a picker is opened.
function openPicker(element) {
  return test_driver.bless("show picker")
  .then(() => requestAnimationFramePromise())
  .then(() => {
    if (element instanceof HTMLSelectElement) {
      // Select is the only control with a picker that (currently)
      // doesn't support showPicker().
      element.focus();
      const isMac = navigator.platform.toUpperCase().indexOf('MAC')>=0;
      let activation_key;
      if (isMac) {
        activation_key = '\uE00A' + '\uE015'; // Alt-Arrow-Down
      } else {
        activation_key = ' '; // Spacebar
      }
      return test_driver.send_keys(element, activation_key);
    } else {
      return element.showPicker(); // Should work for file, color, and date/time
    }
  })
  .then(() => {
      if (!window.internals)
        throw 'Internals not available';
      return waitUntilOpen(window.internals.pagePopupWindow);
    })
  .then(() => requestAnimationFramePromise(window.internals.pagePopupWindow))
  .then(() => requestAnimationFramePromise())
  .then(() => requestAnimationFramePromise(window.internals.pagePopupWindow))
  .then(() => requestAnimationFramePromise());
}

function attemptToClosePicker(element) {
  element.blur(); // Sometimes works to close the picker
  internals.endColorChooser(element); // Works for color only
  return eventSender.keyDown('Escape'); // Hitting esc could close file picker
}

// Opens the picker on the given element, waits for it to be
// displayed, and ends the test. This function also enables
// pixel result output, and will add any errors to the visible
// output. If runAfterOpening is provided, it will be called
// after the picker is opened, and before several rAF() calls.
function openPickerAppearanceOnly(element, runAfterOpening) {
  function showError(e) {
    attemptToClosePicker(element);
    const errorMessage = document.createElement('pre');
    errorMessage.innerText = 'FAIL: \n' + e + '\n\n';
    element.parentNode.insertBefore(errorMessage,element);
  }
  if (!window.testRunner) {
    return showError('testRunner is required');
  }
  testRunner.waitUntilDone();
  testRunner.setShouldGeneratePixelResults(true);

  openPicker(element)
    .then(() => {
      if (runAfterOpening !== undefined) {
        return runAfterOpening();
      }
    })
    .then(() => requestAnimationFramePromise())
    .then(() => requestAnimationFramePromise())
    .then(() => testRunner.notifyDone())
    .catch((e) => {
      showError(e);
      testRunner.notifyDone();
    });
}

// Do NOT use openPickerDeprecatedJsTest for any new tests. See crbug.com/1299212.
// This is here to support old tests only.
let waitedForLoad=false;
function openPickerDeprecatedJsTest(element, successCallback, failureCallback) {
  if (!waitedForLoad) {
    waitedForLoad = true;
    window.jsTestIsAsync = true;
    window.onload = () => {
      openPickerDeprecatedJsTest(element, successCallback, failureCallback);
    };
    return;
  }
  openPicker(element)
  .then(() => requestAnimationFramePromise())
  .then(() => requestAnimationFramePromise())
  .then(() => {
    successCallback();
  })
  .catch((e) => {
    if (failureCallback !== undefined) {
      failureCallback(e);
    } else {
      testFailed('Error: ' + e);
      finishJSTest();
    }
  });
}
function waitUntilClosingDeprecatedJsTest(callback) {
  setTimeout(callback, 1);
}

// Enable lang attribute aware UI for all controls.
if (window.internals)
  internals.runtimeFlags.langAttributeAwareFormControlUIEnabled = true;
