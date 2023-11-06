// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

// Timer for automatic update in monitoring mode.
let fetchDiffScheduler: number|null = null;

// Contains names for expanded histograms.
const expandedEntries: Set<string> = new Set();

// Whether the page is in Monitoring mode.
let inMonitoringMode: boolean = false;

/**
 * Returns a boolean that will be true when histogram from subprocesses should
 * be included.
 */
function includeSubprocessMetrics(): boolean {
  const checkbox = getRequiredElement<HTMLInputElement>('subprocess_checkbox');
  return checkbox.checked;
}

/** Sends a request to the given handler. */
function sendRequest(handlerName: string): Promise<any> {
  return sendWithPromise(handlerName, getQuery(), includeSubprocessMetrics());
}

/**
 * Initiates the request for histograms.
 */
function requestHistograms() {
  sendRequest('requestHistograms').then(addHistograms);
}

/** Clears all loaded histograms on the webpage. */
function clearHistograms(): void {
  getRequiredElement('histograms').innerHTML = window.trustedTypes!.emptyHTML;
}

/** Makes the subprocess checkbox disabled, and sets a tooltip. */
function disableSubprocessCheckbox(): void {
  const subprocessCheckbox =
      getRequiredElement<HTMLInputElement>('subprocess_checkbox');
  subprocessCheckbox.disabled = true;
  subprocessCheckbox.title = 'Checkbox is disabled in Monitoring Mode. ' +
      'To enable, switch to Histogram Mode first.';
}

/** Makes the subprocess checkbox enabled. */
function enableSubprocessCheckbox(): void {
  const subprocessCheckbox =
      getRequiredElement<HTMLInputElement>('subprocess_checkbox');
  subprocessCheckbox.disabled = false;
  subprocessCheckbox.removeAttribute('title');
}

/**
 * Starts monitoring histograms.
 * This will get a histogram snapshot as the base to be diffed against.
 */
function startMonitoring() {
  const stopButton = getRequiredElement<HTMLButtonElement>('stop');
  stopButton.disabled = false;
  stopButton.textContent = 'Stop';
  disableSubprocessCheckbox();
  clearHistograms();
  sendRequest('startMonitoring').then(fetchDiff);
}

/**
 * Schedules the fetching of histogram diff (after 1000ms) and rendering it.
 * This will also recursively call the next fetchDiff() to periodically update
 * the page.
 */
function fetchDiff() {
  fetchDiffScheduler = setTimeout(function() {
    sendRequest('fetchDiff').then(addHistograms).then(fetchDiff);
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
  getRequiredElement('accumulating_section').style.display = 'none';
  getRequiredElement('monitoring_section').style.display = 'block';
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
  getRequiredElement('accumulating_section').style.display = 'block';
  getRequiredElement('monitoring_section').style.display = 'none';
  clearHistograms();
  enableSubprocessCheckbox();
  expandedEntries.clear();
  requestHistograms();
}

/**
 * Callback function when users click the stop button in monitoring mode.
 */
function stopMonitoring() {
  if (fetchDiffScheduler) {
    clearTimeout(fetchDiffScheduler);
    fetchDiffScheduler = null;
  }
  const stopButton = getRequiredElement<HTMLButtonElement>('stop');
  stopButton.disabled = true;
  stopButton.textContent = 'Stopped';
}

/**
 * Returns if monitoring mode is stopped.
 */
export function monitoringStopped(): boolean {
  return inMonitoringMode && !fetchDiffScheduler;
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
  clearHistograms();
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
    getRequiredElement('histograms').appendChild(clone);
  }
  getRequiredElement('histograms')
      .dispatchEvent(new CustomEvent('histograms-updated-for-test'));
}

/**
 * Returns the histograms as a formatted string.
 */
export function generateHistogramsAsText() {
  // Expanded/collapsed status is reflected in the text.
  return getRequiredElement('histograms').innerText;
}

/**
 * Callback function when users click the Download button.
 */
function downloadHistograms() {
  const text = generateHistogramsAsText();
  if (text) {
    const file = new Blob([text], {type: 'text/plain'});
    const a = document.createElement('a');
    a.href = URL.createObjectURL(file);
    a.download = 'histograms.txt';
    a.click();
  }
}

/**
 * Load the initial list of histograms.
 */
document.addEventListener('DOMContentLoaded', function() {
  getRequiredElement('refresh').onclick = requestHistograms;
  getRequiredElement('download').onclick = downloadHistograms;
  getRequiredElement('enable_monitoring').onclick = enableMonitoring;
  getRequiredElement('disable_monitoring').onclick = disableMonitoring;
  getRequiredElement('stop').onclick = stopMonitoring;
  getRequiredElement('subprocess_checkbox').onclick = requestHistograms;

  requestHistograms();
});

/**
 * Reload histograms when the "#abc" in "chrome://histograms/#abc" changes.
 */
window.onhashchange = requestHistograms;
