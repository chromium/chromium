// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

// Timer for automatic update in monitoring mode.
let fetchDiffScheduler: number|null = null;

// Contains names for expanded histograms.
const expandedEntries: Set<string> = new Set();

// Whether the page is in Monitoring mode.
let inMonitoringMode: boolean = false;

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
 *
 * For example, if the URL is
 *   - "chrome://histograms/#abc" or
 *   - "chrome://histograms/abc"
 * then the query is "abc". The "#" format is canonical. The bare format is
 * historical. "Blink.ImageDecodeTimes.Png" is a valid histogram name but the
 * ".Png" pathname suffix can cause the bare histogram page to be served as
 * image/png instead of text/html.
 *
 * See WebUIDataSourceImpl::GetMimeType in
 * content/browser/webui/web_ui_data_source_impl.cc for Content-Type sniffing.
 */
function getQuery() {
  if (document.location.hash) {
    return document.location.hash.substring(1);
  } else if (document.location.pathname) {
    return document.location.pathname.substring(1);
  }
  return '';
}

/**
 * Callback function when users switch to Monitoring mode.
 */
function enableMonitoring() {
  inMonitoringMode = true;
  $('accumulating_section').style.display = 'none';
  $('monitoring_section').style.display = 'block';
  $('histograms').innerHTML =
      window.trustedTypes!.emptyHTML as unknown as string;
  expandedEntries.clear();
  startMonitoring();
}

/**
 * Callback function when users switch away from Monitoring mode.
 */
function disableMonitoring() {
  inMonitoringMode = false;
  if (fetchDiffScheduler) {
    clearTimeout(fetchDiffScheduler);
    fetchDiffScheduler = null;
  }
  $('accumulating_section').style.display = 'block';
  $('monitoring_section').style.display = 'none';
  $('histograms').innerHTML =
      window.trustedTypes!.emptyHTML as unknown as string;
  expandedEntries.clear();
  requestHistograms();
}

function onHistogramHeaderClick(event: Event) {
  const headerElement = event.currentTarget as HTMLElement;
  const name = headerElement.getAttribute('histogram-name');
  assert(name);
  const shouldExpand = !expandedEntries.has(name);
  if (shouldExpand) {
    expandedEntries.add(name);
  } else {
    expandedEntries.delete(name);
  }
  setExpanded(headerElement.parentElement!, shouldExpand);
}

/**
 * Expands or collapses a histogram node.
 * @param histogramNode the histogram element to expand or collapse
 * @param expanded whether to expand or collapse the node
 */
function setExpanded(histogramNode: HTMLElement, expanded: boolean) {
  const body = histogramNode.querySelector<HTMLElement>('.histogram-body');
  const expand = histogramNode.querySelector<HTMLElement>('.expand');
  const collapse = histogramNode.querySelector<HTMLElement>('.collapse');
  assert(body && expand && collapse);

  body.style.display = expanded ? 'block' : 'none';
  expand.style.display = expanded ? 'none' : 'inline';
  collapse.style.display = expanded ? 'inline' : 'none';
}

interface Histogram {
  name: string;
  header: string;
  body: string;
}

/**
 * Callback from backend with the list of histograms. Builds the UI.
 * @param histograms A list of name, header and body strings representing
 *     histograms.
 */
function addHistograms(histograms: Histogram[]) {
  $('histograms').innerHTML =
      window.trustedTypes!.emptyHTML as unknown as string;
  // TBD(jar) Write a nice HTML bar chart, with divs an mouse-overs etc.
  for (const histogram of histograms) {
    const {name, header, body} = histogram;
    const template =
        document.body.querySelector<HTMLTemplateElement>('#histogram-template');
    assert(template);
    const clone = template.content.cloneNode(true) as HTMLElement;
    const headerNode = clone.querySelector<HTMLElement>('.histogram-header');
    assert(headerNode);
    headerNode.setAttribute('histogram-name', name);
    headerNode.onclick = onHistogramHeaderClick;
    clone.querySelector('.histogram-header-text')!.textContent = header;
    const link =
        clone.querySelector<HTMLAnchorElement>('.histogram-header-link');
    assert(link);
    link.href = '/#' + name;
    // Don't run expand/collapse handler on link click.
    link.onclick = (e: Event) => e.stopPropagation();
    clone.querySelector('p')!.textContent = body;
    // If we are not in monitoring mode, default to expand.
    if (!inMonitoringMode) {
      expandedEntries.add(name);
    }
    // In monitoring mode, we want to preserve the expanded/collapsed status
    // between reloads.
    setExpanded(clone, expandedEntries.has(name));
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

/**
 * Reload histograms when the "#abc" in "chrome://histograms/#abc" changes.
 */
window.onhashchange = requestHistograms;
