// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InteractionManager} from '//ios/chrome/browser/web/model/web_performance_metrics/resources/interaction_manager.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

const EVENT_TYPES = [
  'mousedown',
  'keydown',
  'touchstart',
  'pointerdown',
];
const FIRST_CONTENTFUL_PAINT = 'first-contentful-paint';
const WEB_PERFORMANCE_METRICS_HANDLER_NAME = 'WebPerformanceMetricsHandler';

let loadedFromCache = false;
let inpObserver: PerformanceObserver|null = null;

// TODO(crbug.com/525390779): Enable INP monitoring once bugs have been ironed
// out.
const isINPEnabled = false;

// Manager to handle the Interaction To Next Paint (INP) metric.
const interactionManager = new InteractionManager();

// Sends the First Contentful Paint time for each
// frame in a website to the browser. Due to WebKit's
// implementation of First Contentful Paint, this
// will only be called for the main frame and
// subframes that are same-origin relative to the
// main frame.
function processPaintEvents(paintEvents: PerformanceObserverEntryList,
                            observer: PerformanceObserver): void {
  // The performance.timing.navigationStart property has been deprecated.
  // TODO(crbug.com/40806748)
  for (const event of paintEvents.getEntriesByName(FIRST_CONTENTFUL_PAINT)){
    const response = {
      'metric': 'FirstContentfulPaint',
      'frameNavigationStartTime': performance.timing.navigationStart,
      'value': event.startTime,
    };

    sendWebKitMessage(WEB_PERFORMANCE_METRICS_HANDLER_NAME, response);

    observer.disconnect();
  }
}

// Processes Event Timing entries to collect interaction durations for INP.
function processINPEvents(eventEntries: PerformanceObserverEntryList): void {
  for (const eventTiming of eventEntries.getEntries() as
       PerformanceEventTiming[]) {
    // Ignore entries without an interaction ID, such as scroll and zoom.
    if (eventTiming.interactionId) {
      interactionManager.record(
          eventTiming.interactionId, eventTiming.duration);
    }
  }
}

// Sends INP data for the frame upon page unload/hide.
// Note that the INP metric will be calculated later when the WebState is
// destroyed or the page is navigated away from. This only sends to the browser
// the data needed to calculate the metric.
function sendINPData(): void {
  if (!isINPEnabled || interactionManager.totalCount === 0) {
    return;
  }
  const response = {
    'metric': 'InteractionToNextPaint',
    'durations': interactionManager.getLongestDurations(),
    'interactionCount': interactionManager.totalCount,
    'frameId': gCrWeb.getFrameId(),
  };
  sendWebKitMessage(WEB_PERFORMANCE_METRICS_HANDLER_NAME, response);
}

// Sends the First Input Delay time for
// each frame in a website to the browser.
function processInputEvent(inputEvent: Event): void {
  const currentTime = performance.now();
  const delta = currentTime - inputEvent.timeStamp;
  const response = {
    'metric': 'FirstInputDelay',
    'value': delta,
    'cached': loadedFromCache,
  };

  sendWebKitMessage(
    WEB_PERFORMANCE_METRICS_HANDLER_NAME,
    response);

  EVENT_TYPES.forEach((type) => {
    window.removeEventListener(type, processInputEvent, { capture: true });
  });
}

// Because JavaScript files are not rerun when
// a web page is loaded from the back/forward
// cache, this function re-registers the
// event listeners to capture and forward
// the Web Performance Metrics back to the
// browser.
function processPageShowEvent(pageshow: PageTransitionEvent): void {
  if (pageshow.persisted) {
    loadedFromCache = true;
    registerInputEventListeners();
  }
}

// Unregisters passive event listeners and flushes metrics on pagehide.
function processPageHideEvent(): void {
  // Sends the INP data for the frame on pagehide, so we don't
  // send the data on every interaction.
  sendINPData();

  EVENT_TYPES.forEach((type) => {
    window.removeEventListener(type, processInputEvent, { capture: true });
  });
  loadedFromCache = false;
}

// Register PerformanceObserver to observe 'paint' events
// Once the PerformanceObserver receives the
// 'first-contentful-paint' event, it captures the time of the
// event and forwards the result to the browser.
function registerPerformanceObserver(): void {
  const observer = new PerformanceObserver(processPaintEvents);
  observer.observe({ entryTypes : ['paint'] });
}

// Register PerformanceObserver to observe 'event' timing entries for INP.
function registerINPObserver(): void {
  if (!isINPEnabled) {
    return;
  }
  try {
    inpObserver = new PerformanceObserver(processINPEvents);
    inpObserver.observe(
        {type: 'event', buffered: true, durationThreshold: 16} as
        PerformanceObserverInit);
  } catch (e) {
    inpObserver = null;
  }
}

// Registers a passive event listener for each predefined
// event type. Once the event listener receives an event,
// it calculates the first input delay and forwards the
// result ot the browser.
function registerInputEventListeners(): void {
  EVENT_TYPES.forEach((type) => {
    window.addEventListener(type,
                             processInputEvent,
                             {capture: true,
                              passive: true});
  });
}

// Registers passive event listeners for the pageshow
// and pagehide events
function registerPageCacheListeners(): void {
  window.addEventListener('pageshow',
                           processPageShowEvent,
                           {capture: true,
                           passive: true});

  window.addEventListener('pagehide',
                           processPageHideEvent,
                           { capture: true, passive: true});
}

registerPerformanceObserver();
registerINPObserver();
registerInputEventListeners();
registerPageCacheListeners();
