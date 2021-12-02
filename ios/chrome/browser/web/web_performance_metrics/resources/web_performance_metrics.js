// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('__crWeb.webPerformanceMetrics');

/** Beginning of anonymous object */
(function() {
  const FIRST_CONTENTFUL_PAINT = 'first-contentful-paint';
  const WEB_PERFORMANCE_METRICS_HANDLER_NAME = 'WebPerformanceMetricsHandler';

  // Sends the First Contentful Paint time for each
  // frame in a website to the browser. Due to WebKit's
  // implementation of First Contentful Paint, this
  // will only be called for the main frame and
  // subframes that are same-origin relative to the
  // main frame.
  function processPaintEvents(paintEvents, observer) {
    for (const event of paintEvents.getEntriesByName(FIRST_CONTENTFUL_PAINT)){
      // The performance.timing.navigationStart property has been deprecated.
      // See https://crbug.com/1273083
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

  // Register PerformanceObserver to observe 'paint' events
  // Once the PerformanceObserver receives the
  // 'first-contentful-paint' event, capture the time of the
  // event and print it out.
  function registerPerformanceObserver(){
    let observer = new PerformanceObserver(processPaintEvents);
    observer.observe({ entryTypes : ['paint'] });
  }

  registerPerformanceObserver();
}());