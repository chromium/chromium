// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const EVENT_TYPES = [
  'mousedown',
  'keydown',
  'touchstart',
  'pointerdown'
];
const FIRST_CONTENTFUL_PAINT = 'first-contentful-paint';
const WEB_PERFORMANCE_METRICS_HANDLER_NAME = 'WebPerformanceMetricsHandler';

let loadedFromCache = false;

// Sends the First Contentful Paint time for each
// frame in a website to the browser. Due to WebKit's
// implementation of First Contentful Paint, this
// will only be called for the main frame and
// subframes that are same-origin relative to the
// main frame.
function processPaintEvents(paintEvents, observer) {
  for (const event of paintEvents.getEntriesByName(FIRST_CONTENTFUL_PAINT)){
    // The performance.timing.navigationStart property has been deprecated.
    // TODO(crbug.com/1273083)
    let response = {
      'metric' : 'FirstContentfulPaint',
      'frameNavigationStartTime' : performance.timing.navigationStart,
      'value'  : event.startTime,
    }

    __gCrWeb.common.sendWebKitMessage(
        WEB_PERFORMANCE_METRICS_HANDLER_NAME,
        response);

    observer.disconnect();
  }
}

// Sends the First Input Delay time for
// each frame in a website to the browser.
function processInputEvent(inputEvent) {
  let currentTime = performance.now();
  let delta = currentTime - inputEvent.timeStamp;
  let response = {
    'metric' : 'FirstInputDelay',
    'value' : delta,
    'cached' : loadedFromCache
  }

  __gCrWeb.common.sendWebKitMessage(
    WEB_PERFORMANCE_METRICS_HANDLER_NAME,
    response);

  EVENT_TYPES.forEach((type) => {
    removeEventListenerFromWindow(type, processInputEvent, { capture: true });
  });
}

// Because JavaScript files are not rerun when
// a web page is loaded from the back/forward
// cache, this function re-registers the
// event listeners to capture and forward
// the Web Performance Metrics back to the
// browser.
function processPageShowEvent(pageshow) {
  if (pageshow.persisted) {
    loadedFromCache = true;
    registerInputEventListeners();
  }
}

// Unregisters the passive event listeners
// used for collecting the First Input Delay
// upon the user navigating away from the
// webpage
function processPageHideEvent() {
  EVENT_TYPES.forEach((type) => {
    removeEventListenerFromWindow(type, processInputEvent, { capture: true });
  });
  loadedFromCache = false;
}

// Register PerformanceObserver to observe 'paint' events
// Once the PerformanceObserver receives the
// 'first-contentful-paint' event, it captures the time of the
// event and forwards the result to the browser.
function registerPerformanceObserver(){
  let observer = new PerformanceObserver(processPaintEvents);
  observer.observe({ entryTypes : ['paint'] });
}

// Registers a passive event listener for each predefined
// event type. Once the event listener receives an event,
// it calculates the first input delay and forwards the
// result ot the browser.
function registerInputEventListeners() {
  EVENT_TYPES.forEach((type) => {
    addEventListenerToWindow(type,
                             processInputEvent,
                             {capture: true,
                              passive: true});
  });
}

// Registers passive event listeners for the pageshow
// and pagehide events
function registerPageCacheListeners() {
  addEventListenerToWindow('pageshow',
                           processPageShowEvent,
                           {capture: true,
                           passive: true});

  addEventListenerToWindow('pagehide',
                           processPageHideEvent,
                           { capture: true, passive: true});
}

// Wrapper function for adding an event listener to the
// window.
function addEventListenerToWindow(type, callback, options) {
  window.addEventListener(type, callback, options);
}

// Wrapper function for removing an event listener from the
// window.
function removeEventListenerFromWindow(type, callback, options) {
  window.removeEventListener(type, callback, options);
}

registerPerformanceObserver();
registerInputEventListeners();
registerPageCacheListeners();
