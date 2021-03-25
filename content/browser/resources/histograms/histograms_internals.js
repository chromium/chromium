// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

// Timer for automatic update in monitoring mode.
let fetchDiffScheduler = null;

/**
 * Initiates the request for histograms.
 */
function requestHistograms() {
  sendWithPromise('requestHistograms', getQuery()).then(addHistograms);
}

/**
 * Starts monitoring histograms.
 * This will get a histogram snapshot as the base to be diffed against.
 */
function startMonitoring() {
  sendWithPromise('startMonitoring', getQuery()).then(fetchDiff);
}

/**
 * Schedules the fetching of histogram diff (after 1000ms) and rendering it.
 * This will also recursively call the next fetchDiff() to periodically updtate
 * the page.
 */
function fetchDiff() {
  fetchDiffScheduler = setTimeout(function() {
    sendWithPromise('fetchDiff', getQuery())
        .then(addHistograms)
        .then(fetchDiff);
  }, 1000);
}

/**
 * Gets the query string from the URL.
 * For example, if the URL is chrome://histograms/abc, then query is "abc".
 */
function getQuery() {
  if (document.location.pathname) {
    return document.location.pathname.substring(1);
  }
  return '';
}

/**
 * Callback function when users switch to Monitoring mode.
 */
function enableMonitoring() {
  $('accumulating_section').style.display = 'none';
  $('monitoring_section').style.display = 'block';
  $('histograms').innerHTML = trustedTypes.emptyHTML;
  startMonitoring();
}

/**
 * Callback function when users switch away from Monitoring mode.
 */
function disableMonitoring() {
  if (fetchDiffScheduler) {
    clearTimeout(fetchDiffScheduler);
    fetchDiffScheduler = null;
  }
  $('accumulating_section').style.display = 'block';
  $('monitoring_section').style.display = 'none';
  $('histograms').innerHTML = trustedTypes.emptyHTML;
  requestHistograms();
}

/**
 * Callback from backend with the list of histograms. Builds the UI.
 * @param {!Array<{header: string, body: string}>} histograms A list
 *     of header and body strings representing histograms.
 */
function addHistograms(histograms) {
  $('histograms').innerHTML = trustedTypes.emptyHTML;
  // TBD(jar) Write a nice HTML bar chart, with divs an mouse-overs etc.
  for (const histogram of histograms) {
    const {header, body} = histogram;
    const clone = $('histogram-template').content.cloneNode(true);

    clone.querySelector('h4').textContent = header;
    clone.querySelector('p').textContent = body;
    $('histograms').appendChild(clone);
  }
  $('histograms').dispatchEvent(new CustomEvent('histograms-updated-for-test'));
}

/**
 * Load the initial list of histograms.
 */
document.addEventListener('DOMContentLoaded', function() {
  $('refresh').onclick = requestHistograms;
  $('enable_monitoring').onclick = enableMonitoring;
  $('disable_monitoring').onclick = disableMonitoring;
  requestHistograms();
});
