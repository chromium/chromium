// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

/**
 * Initiates the request for histograms.
 */
function requestHistograms() {
  let query = '';
  if (document.location.pathname) {
    query = document.location.pathname.substring(1);
  }
  sendWithPromise('requestHistograms', query).then(addHistograms);
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

  requestHistograms();
});
