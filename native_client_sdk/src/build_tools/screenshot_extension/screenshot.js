// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var screenshot = (function() {
  /** A map of id to pending callback. */
  var callbackMap = {};

  /** An array of queued requests. They will all be executed when the
   * background page injects screen code into this page
   */
  var queuedRequests = [];

  /** The next id to assign. Used for mapping id to callback. */
  var nextId = 0;

  /** This is set to true when the background page injects screenshot code into
   * this page
   */
  var extensionInjected = false;

  /** Generate a new id, which maps to the given callbacks.
   *
   * @param {function(string)} onSuccess
   * @param {function(string)} onError
   * @return {number} The id.
   */
  function addCallback(onSuccess, onError) {
    var id = nextId++;
    callbackMap[id] = [onSuccess, onError];
    return id;
  }

  /** Call the callback mapped to |id|.
   *
   * @param {number} id
   * @param {boolean} success true to call the success callback, false for the
   *                          error callback.
   * @param {...} A variable number of arguments to pass to the callback.
   */
  function callCallback(id, success) {
    var callbacks = callbackMap[id];
    if (!callbacks) {
      console.log('Unknown id: ' + id);
      return;
    }

    delete callbackMap[id];
    var callback = success ? callbacks[0] : callbacks[1];
    if (callback)
      callback(Array.prototype.slice.call(arguments, 2));
  }

  /** Post a message to take a screenshot.
   *
   * This message will be enqueued if the extension has not yet injected the
   * screenshot code.
   *
   * @param {number} id An id to associate with this request. When the
   *                    screenshot is complete, the background page will return
   *                    a result with this id.
   */
  function postScreenshotMessage(id) {
    if (!extensionInjected) {
      queuedRequests.push(id);
      return;
    }

    window.postMessage({id: id, target: 'background'}, '*');
  }

  /** Post all queued screenshot requests.
   *
   * Should only be called after the screenshot code has been injected by the
   * background page.
   */
  function postQueuedRequests() {
    for (var i = 0; i < queuedRequests.length; ++i) {
      var id = queuedRequests[i];
      postScreenshotMessage(id);
    }
    queuedRequests = [];
  }

  /** Predicate whether the extension has injected code yet.
   *
   * @return {boolean}
   */
  function isInjected() {
    // NOTE: This attribute name must match the one in injected.js.
    return document.body &&
        document.body.getAttribute('screenshot_extension_injected');
  }

  /** Start an interval that polls for when the extension has injected code
   * into this page.
   *
   * The extension first adds a postMessage handler to listen for requests,
   * then adds an attribute to the body element. If we see this attribute, we
   * know the listener is ready.
   */
  function pollForInjection() {
    var intervalId = window.setInterval(function() {
      if (!isInjected())
        return;

      // Finally injected!
      window.clearInterval(intervalId);
      extensionInjected = true;
      postQueuedRequests();
    }, 100);  // Every 100ms.
  }

  // Add a postMessage listener for when the injected code returns a result
  // from the background page.
  window.addEventListener('message', function(event) {
    // If the message comes from another window, or is outbound (i.e.
    // event.data.target === 'background'), ignore it.
    if (event.source !== window || event.data.target !== 'page')
      return;

    var success = event.data.error === undefined;
    callCallback(event.data.id, success, event.data.data);
  }, false);

  if (isInjected())
    extensionInjected = true;
  else
    pollForInjection();

  // Public functions.

  /** Capture the current visible area of the tab as a PNG.
   *
   * If the request succeeds, |onSuccess| will be called with one parameter:
   * the image encoded as a data URL.
   *
   * If the request fails, |onError| will be called with one parameter: an
   * informational error message.
   *
   * @param {function(string)} onSuccess The success callback.
   * @param {function(string)} onError The error callback.
   */
  function captureTab(onSuccess, onError) {
    var id = addCallback(onSuccess, onError);
    postScreenshotMessage(id);
  }

  /** Capture the current visible area of a given element as a PNG.
   *
   * If the request succeeds, |onSuccess| will be called with one parameter:
   * the image encoded as a data URL.
   *
   * If the request fails, |onError| will be called with one parameter: an
   * informational error message.
   *
   * @param {Element} element The element to capture.
   * @param {function(string)} onSuccess The success callback.
   * @param {function(string)} onError The error callback.
   */
  function captureElement(element, onSuccess, onError) {
    var elementRect = element.getBoundingClientRect();
    var elX = elementRect.left;
    var elY = elementRect.top;
    var elW = elementRect.width;
    var elH = elementRect.height;

    function onScreenCaptured(dataURL) {
      // Create a canvas of the correct size.
      var canvasEl = document.createElement('canvas');
      canvasEl.setAttribute('width', elW);
      canvasEl.setAttribute('height', elH);
      var ctx = canvasEl.getContext('2d');

      var imgEl = new Image();
      imgEl.onload = function() {
        // Draw only the element region of the image.
        ctx.drawImage(imgEl, elX, elY, elW, elH, 0, 0, elW, elH);

        // Extract the canvas to a new data URL, and return it via the callback.
        onSuccess(canvasEl.toDataURL());
      };

      // Load the full screenshot into imgEl.
      imgEl.src = dataURL;
    }

    var id = addCallback(onScreenCaptured, onError);
    postScreenshotMessage(id);
  }

  return {
    captureTab: captureTab,
    captureElement: captureElement
  };
})();
