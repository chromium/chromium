/* Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {assert} from 'chrome://resources/js/assert.js';
import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util.js';

// Type definitions for custom types used through the file.
type TailoredVerdictOverrideFormElements =
    HTMLFormControlsCollection & {tailored_verdict_type: HTMLInputElement};
type TailoredVerdictOverrideForm =
    HTMLFormElement & {elements: TailoredVerdictOverrideFormElements};
interface ReportingResult {
  message: string;
  time: string;
}
interface DeepScanResult {
  request: string;
  request_time: string;
  token: string;
  response: string;
  response_time: string;
  response_status: string;
}

/**
 * Asks the C++ SafeBrowsingUIHandler to get the lists of Safe Browsing
 * ongoing experiments and preferences.
 * The SafeBrowsingUIHandler should reply to addExperiment() and
 * addPreferences() (below).
 */
function initialize() {
  sendWithPromise('getExperiments', [])
      .then((experiments) => addExperiments(experiments));
  sendWithPromise('getPrefs', []).then((prefs) => addPrefs(prefs));
  sendWithPromise('getPolicies', []).then((policies) => addPolicies(policies));
  sendWithPromise('getCookie', []).then((cookie) => addCookie(cookie));
  sendWithPromise('getSavedPasswords', [])
      .then((passwords) => addSavedPasswords(passwords));
  sendWithPromise('getDatabaseManagerInfo', []).then(function(databaseState) {
    const fullHashCacheState = databaseState.splice(-1, 1);
    addDatabaseManagerInfo(databaseState);
    addFullHashCacheInfo(fullHashCacheState);
  });

  sendWithPromise('getDownloadUrlsChecked', [])
      .then((urlsChecked: string[]) => {
        urlsChecked.forEach(function(urlAndResult) {
          addDownloadUrlChecked(urlAndResult);
        });
      });
  addWebUiListener(
      'download-url-checked-update', function(urlAndResult: string) {
        addDownloadUrlChecked(urlAndResult);
      });

  sendWithPromise('getSentClientDownloadRequests', [])
      .then((sentClientDownloadRequests: string[]) => {
        sentClientDownloadRequests.forEach(function(cdr) {
          addSentClientDownloadRequestsInfo(cdr);
        });
      });
  addWebUiListener(
      'sent-client-download-requests-update', function(result: string) {
        addSentClientDownloadRequestsInfo(result);
      });

  sendWithPromise('getReceivedClientDownloadResponses', [])
      .then((receivedClientDownloadResponses: string[]) => {
        receivedClientDownloadResponses.forEach(function(cdr) {
          addReceivedClientDownloadResponseInfo(cdr);
        });
      });
  addWebUiListener(
      'received-client-download-responses-update', function(result: string) {
        addReceivedClientDownloadResponseInfo(result);
      });

  sendWithPromise('getSentClientPhishingRequests', [])
      .then((sentClientPhishingRequests: string[]) => {
        sentClientPhishingRequests.forEach(function(cpr: string) {
          addSentClientPhishingRequestsInfo(cpr);
        });
      });
  addWebUiListener(
      'sent-client-phishing-requests-update', function(result: string) {
        addSentClientPhishingRequestsInfo(result);
      });

  sendWithPromise('getReceivedClientPhishingResponses', [])
      .then((receivedClientPhishingResponses: string[]) => {
        receivedClientPhishingResponses.forEach(function(cpr) {
          addReceivedClientPhishingResponseInfo(cpr);
        });
      });
  addWebUiListener(
      'received-client-phishing-responses-update', function(result: string) {
        addReceivedClientPhishingResponseInfo(result);
      });

  sendWithPromise('getSentCSBRRs', []).then((sentCSBRRs: string[]) => {
    sentCSBRRs.forEach(function(csbrr) {
      addSentCSBRRsInfo(csbrr);
    });
  });
  addWebUiListener('sent-csbrr-update', function(result: string) {
    addSentCSBRRsInfo(result);
  });

  sendWithPromise('getSentHitReports', []).then((sentHitReports: string[]) => {
    sentHitReports.forEach(function(hitReports) {
      addSentHitReportsInfo(hitReports);
    });
  });
  addWebUiListener('sent-hit-report-list', function(result: string) {
    addSentHitReportsInfo(result);
  });

  sendWithPromise('getPGEvents', []).then((pgEvents: ReportingResult[]) => {
    pgEvents.forEach(function(pgEvent) {
      addPGEvent(pgEvent);
    });
  });
  addWebUiListener('sent-pg-event', function(result: ReportingResult) {
    addPGEvent(result);
  });

  sendWithPromise('getSecurityEvents', [])
      .then((securityEvents: ReportingResult[]) => {
        securityEvents.forEach(function(securityEvent) {
          addSecurityEvent(securityEvent);
        });
      });
  addWebUiListener('sent-security-event', function(result: ReportingResult) {
    addSecurityEvent(result);
  });

  sendWithPromise('getPGPings', []).then((pgPings: string[][]) => {
    pgPings.forEach(function(pgPing) {
      addPGPing(pgPing);
    });
  });
  addWebUiListener('pg-pings-update', function(result: string[]) {
    addPGPing(result);
  });

  sendWithPromise('getPGResponses', []).then((pgResponses: string[][]) => {
    pgResponses.forEach(function(pgResponse) {
      addPGResponse(pgResponse);
    });
  });
  addWebUiListener('pg-responses-update', function(result: string[]) {
    addPGResponse(result);
  });

  sendWithPromise('getURTLookupPings', [])
      .then((urtLookupPings: string[][]) => {
        urtLookupPings.forEach(function(urtLookupPing) {
          addURTLookupPing(urtLookupPing);
        });
      });
  addWebUiListener('urt-lookup-pings-update', function(result: string[]) {
    addURTLookupPing(result);
  });

  sendWithPromise('getURTLookupResponses', [])
      .then((urtLookupResponses: string[][]) => {
        urtLookupResponses.forEach(function(urtLookupResponse) {
          addURTLookupResponse(urtLookupResponse);
        });
      });
  addWebUiListener('urt-lookup-responses-update', function(result: string[]) {
    addURTLookupResponse(result);
  });

  sendWithPromise('getHPRTLookupPings', [])
      .then((hprtLookupPings: string[][]) => {
        hprtLookupPings.forEach(function(hprtLookupPing: string[]) {
          addHPRTLookupPing(hprtLookupPing);
        });
      });
  addWebUiListener('hprt-lookup-pings-update', function(result: string[]) {
    addHPRTLookupPing(result);
  });

  sendWithPromise('getHPRTLookupResponses', [])
      .then((hprtLookupResponses: string[][]) => {
        hprtLookupResponses.forEach(function(hprtLookupResponse) {
          addHPRTLookupResponse(hprtLookupResponse);
        });
      });
  addWebUiListener('hprt-lookup-responses-update', function(result: string[]) {
    addHPRTLookupResponse(result);
  });

  sendWithPromise('getLogMessages', [])
      .then((logMessages: ReportingResult[]) => {
        logMessages.forEach(function(message) {
          addLogMessage(message);
        });
      });
  addWebUiListener('log-messages-update', function(message: ReportingResult) {
    addLogMessage(message);
  });

  sendWithPromise('getReportingEvents', [])
      .then((reportingEvents: ReportingResult[]) => {
        reportingEvents.forEach(function(reportingEvent) {
          addReportingEvent(reportingEvent);
        });
      });
  addWebUiListener(
      'reporting-events-update', function(reportingEvent: ReportingResult) {
        addReportingEvent(reportingEvent);
      });

  sendWithPromise('getDeepScans', []).then((requests: DeepScanResult[]) => {
    requests.forEach(function(request) {
      addDeepScan(request);
    });
  });
  addWebUiListener(
      'deep-scan-request-update', function(result: DeepScanResult) {
        addDeepScan(result);
      });

  // <if expr="is_android">
  sendWithPromise('getReferringAppInfo', []).then((info: string) => {
    addReferringAppInfo(info);
  });
  // </if>

  const referrerChangeForm = $('get-referrer-chain-form');
  assert(referrerChangeForm);
  referrerChangeForm.addEventListener('submit', addReferrerChain);

  sendWithPromise('getTailoredVerdictOverride', [])
      .then(displayTailoredVerdictOverride);
  addWebUiListener(
      'tailored-verdict-override-update', displayTailoredVerdictOverride);

  const overrideForm =
      $<TailoredVerdictOverrideForm>('tailored-verdict-override-form');
  assert(overrideForm);
  overrideForm.addEventListener('submit', setTailoredVerdictOverride);
  const overrideClear = $('tailored-verdict-override-clear');
  assert(overrideClear);
  overrideClear.addEventListener('click', clearTailoredVerdictOverride);

  // Allow tabs to be navigated to by fragment. The fragment with be of the
  // format "#tab-<tab id>"
  showTab(window.location.hash.substr(5));
  window.onhashchange = function() {
    showTab(window.location.hash.substr(5));
  };

  // When the tab updates, update the anchor
  const tabbox = $('tabbox');
  assert(tabbox);
  tabbox.addEventListener('selected-index-change', e => {
    const tabs = document.querySelectorAll('div[slot=\'tab\']');
    const selectedTab = tabs[e.detail];
    assert(selectedTab);
    window.location.hash = 'tab-' + selectedTab.id;
  }, true);
}

function addExperiments(result: string[]) {
  addContentHelper(result, 'result-template', 'experiments-list', 'span');
}

function addPrefs(result: string[]) {
  addContentHelper(result, 'result-template', 'preferences-list', 'span');
}

function addPolicies(result: string[]) {
  addContentHelper(result, 'result-template', 'policies-list', 'span');
}

function addCookie(result: string[]) {
  const cookiePanel = $('cookie-panel');
  const cookieTemplate = $<HTMLTemplateElement>('cookie-template');
  assert(cookieTemplate);
  const cookieFormatted = cookieTemplate.content.cloneNode(true) as HTMLElement;
  const selectedElements = cookieFormatted.querySelectorAll('.result');
  assert(selectedElements);
  const firstElement = selectedElements[0];
  const secondElement = selectedElements[1];
  const firstResult = result[0];
  const secondResult = result[1];

  assert(cookiePanel);
  assert(firstElement);
  assert(secondElement);
  assert(firstResult);
  assert(secondResult);

  firstElement.textContent = firstResult;
  secondElement.textContent = (new Date(secondResult)).toLocaleString();
  cookiePanel.appendChild(cookieFormatted);
}

function addSavedPasswords(result: string[]) {
  const resLength = result.length;

  for (let i = 0; i < resLength; i += 2) {
    const savedPasswordFormatted = document.createElement('div');
    const suffix = result[i + 1] ? 'GAIA password' : 'Enterprise password';
    savedPasswordFormatted.textContent = `${result[i]} (${suffix})`;
    const savedPasswordsList = $('saved-passwords');
    assert(savedPasswordsList);
    savedPasswordsList.appendChild(savedPasswordFormatted);
  }
}

function addDatabaseManagerInfo(result: string[]) {
  const resLength = result.length;

  for (let i = 0; i < resLength; i += 2) {
    const preferenceListTemplate = $<HTMLTemplateElement>('result-template');
    assert(preferenceListTemplate);
    const preferencesListFormatted =
        preferenceListTemplate.content.cloneNode(true) as HTMLElement;
    const selectedElements = preferencesListFormatted.querySelectorAll('span');
    assert(selectedElements);
    const firstElement = selectedElements[0];
    const secondElement = selectedElements[1];
    assert(firstElement);
    assert(secondElement);

    firstElement.textContent = result[i] + ': ';
    const value = result[i + 1];
    assert(value);
    if (Array.isArray(value)) {
      const blockQuote = document.createElement('blockquote');
      value.forEach(item => {
        const div = document.createElement('div');
        div.textContent = item;
        blockQuote.appendChild(div);
      });
      secondElement.appendChild(blockQuote);
    } else {
      secondElement.textContent = value;
    }
    const databaseInfoList = $('database-info-list');
    assert(databaseInfoList);
    databaseInfoList.appendChild(preferencesListFormatted);
  }
}

// A helper function that formats and adds strings from `result` into the
// template and then appends the template to the parent list.
function addContentHelper(
    result: string[], templateName: string, parentListName: string,
    elementSelector: string) {
  const resLength = result.length;

  for (let i = 0; i < resLength; i += 2) {
    const listTemplate = $<HTMLTemplateElement>(templateName);
    assert(listTemplate);
    const formattedTemplate =
        listTemplate.content.cloneNode(true) as HTMLElement;
    const firstResult = result[i];
    const secondResult = result[i + 1];
    const selectedElements =
        formattedTemplate.querySelectorAll(elementSelector);
    const firstElement = selectedElements[0];
    const secondElement = selectedElements[1];
    const parentList = $(parentListName);

    assert(firstResult !== null && firstResult !== undefined);
    assert(secondResult);
    assert(firstElement);
    assert(secondElement);
    assert(parentList);

    firstElement.textContent = secondResult + ': ';
    secondElement.textContent = firstResult;
    parentList.appendChild(formattedTemplate);
  }
}

function addFullHashCacheInfo(result: string) {
  const cacheInfo = $('full-hash-cache-info');
  assert(cacheInfo);
  cacheInfo.textContent = result;
}

function addDownloadUrlChecked(urlAndResult: string) {
  const logDiv = $('download-urls-checked-list');
  appendChildWithInnerText(logDiv, urlAndResult);
}

function addSentClientDownloadRequestsInfo(result: string) {
  const logDiv = $('sent-client-download-requests-list');
  appendChildWithInnerText(logDiv, result);
}

function addReceivedClientDownloadResponseInfo(result: string) {
  const logDiv = $('received-client-download-response-list');
  appendChildWithInnerText(logDiv, result);
}

function addSentClientPhishingRequestsInfo(result: string) {
  const logDiv = $('sent-client-phishing-requests-list');
  appendChildWithInnerText(logDiv, result);
}

function addReceivedClientPhishingResponseInfo(result: string) {
  const logDiv = $('received-client-phishing-response-list');
  appendChildWithInnerText(logDiv, result);
}

function addSentCSBRRsInfo(result: string) {
  const logDiv = $('sent-csbrrs-list');
  appendChildWithInnerText(logDiv, result);
}

function addSentHitReportsInfo(result: string) {
  const logDiv = $('sent-hit-report-list');
  appendChildWithInnerText(logDiv, result);
}

function addPGEvent(result: ReportingResult) {
  const logDiv = $('pg-event-log');
  const eventFormatted =
      '[' + (new Date(result.time)).toLocaleString() + '] ' + result.message;
  appendChildWithInnerText(logDiv, eventFormatted);
}

function addSecurityEvent(result: ReportingResult) {
  const logDiv = $('security-event-log');
  const eventFormatted =
      '[' + (new Date(result.time)).toLocaleString() + '] ' + result.message;
  appendChildWithInnerText(logDiv, eventFormatted);
}

function insertTokenToTable(tableId: string, token?: string) {
  const table = $<HTMLTableElement>(tableId);
  assert(table);
  const row = table.insertRow();
  row.className = 'content';
  row.id = tableId + '-' + token;
  row.insertCell().className = 'content';
  row.insertCell().className = 'content';
}

function addResultToTable(
    tableId: string, token: string, result: string, position: number) {
  let table = $<HTMLTableRowElement>(`${tableId}-${token}`);
  if (table === null) {
    insertTokenToTable(tableId, token);
    table = $<HTMLTableRowElement>(`${tableId}-${token}`);
  }

  assert(table);
  const cell = table.cells[position];
  assert(cell);
  cell.innerText = result;
}

function addPGPing(result: string[]) {
  addResultToTableHelper('pg-ping-list', result, 0);
}

function addPGResponse(result: string[]) {
  addResultToTableHelper('pg-ping-list', result, 1);
}

function addURTLookupPing(result: string[]) {
  addResultToTableHelper('urt-lookup-ping-list', result, 0);
}

function addURTLookupResponse(result: string[]) {
  addResultToTableHelper('urt-lookup-ping-list', result, 1);
}

function addHPRTLookupPing(result: string[]) {
  addResultToTableHelper('hprt-lookup-ping-list', result, 0);
}

function addHPRTLookupResponse(result: string[]) {
  addResultToTableHelper('hprt-lookup-ping-list', result, 1);
}

// A helper function that ensures there are elements within `results` before
// adding them to a list.
function addResultToTableHelper(
    listName: string, result: Array<string|number>, position: number) {
  const tableId =
      typeof result[0] === 'number' ? (result[0]).toString() : result[0];
  const token = result[1] as string;
  assert(tableId);
  assert(token);

  addResultToTable(listName, tableId, token, position);
}

function addDeepScan(result: DeepScanResult) {
  if (result.request_time !== null) {
    const requestFormatted = '[' +
        (new Date(result.request_time)).toLocaleString() + ']\n' +
        result.request;
    addResultToTable('deep-scan-list', result.token, requestFormatted, 0);
  }

  if (result.response_time != null) {
    if (result.response_status === 'SUCCESS') {
      // Display the response instead
      const resultFormatted = '[' +
          (new Date(result.response_time)).toLocaleString() + ']\n' +
          result.response;
      addResultToTable('deep-scan-list', result.token, resultFormatted, 1);
    } else {
      // Display the error
      const resultFormatted = '[' +
          (new Date(result.response_time)).toLocaleString() + ']\n' +
          result.response_status;
      addResultToTable('deep-scan-list', result.token, resultFormatted, 1);
    }
  }
}

function addLogMessage(result: ReportingResult) {
  const logDiv = $('log-messages');
  const eventFormatted =
      '[' + (new Date(result.time)).toLocaleString() + '] ' + result.message;
  appendChildWithInnerText(logDiv, eventFormatted);
}

function addReportingEvent(result: ReportingResult) {
  const logDiv = $('reporting-events');
  const eventFormatted = result.message;
  appendChildWithInnerText(logDiv, eventFormatted);
}

function appendChildWithInnerText(logDiv: Element|null, text: string) {
  if (!logDiv) {
    return;
  }
  const textDiv = document.createElement('div');
  textDiv.innerText = text;
  logDiv.appendChild(textDiv);
}

function addReferrerChain(ev: Event) {
  // Don't navigate
  ev.preventDefault();

  const referrerChainURL = $<HTMLInputElement>('referrer-chain-url');
  assert(referrerChainURL);
  sendWithPromise('getReferrerChain', referrerChainURL.value)
      .then((response) => {
        const referrerChainContent = $('referrer-chain-content');
        assert(referrerChainContent);
        // TrustedTypes is not supported on iOS
        if (window.trustedTypes) {
          referrerChainContent.innerHTML = window.trustedTypes.emptyHTML;
        } else {
          referrerChainContent.innerHTML = '';
        }
        referrerChainContent.textContent = response;
      });
}

// <if expr="is_android">
function addReferringAppInfo(info: string|null) {
  const referringAppInfo = $('referring-app-info');
  assert(referringAppInfo);
  // TrustedTypes is not supported on iOS
  if (window.trustedTypes) {
    referringAppInfo.innerHTML = window.trustedTypes.emptyHTML;
  } else {
    referringAppInfo.innerHTML = '';
  }
  referringAppInfo.textContent = info;
}
// </if>

// Format the browser's response nicely.
function displayTailoredVerdictOverride(
    response: {status: number, override_value: object}) {
  let displayString = `Status: ${response.status}`;
  if (response.override_value) {
    displayString +=
        `\nOverride value: ${JSON.stringify(response.override_value)}`;
  }
  const overrideContent = $('tailored-verdict-override-content');
  assert(overrideContent);
  // TrustedTypes is not supported on iOS
  if (window.trustedTypes) {
    overrideContent.innerHTML = window.trustedTypes.emptyHTML;
  } else {
    overrideContent.innerHTML = '';
  }
  overrideContent.textContent = displayString;
}

function setTailoredVerdictOverride(e: Event) {
  // Don't navigate
  e.preventDefault();

  const form = $<TailoredVerdictOverrideForm>('tailored-verdict-override-form');
  assert(form);
  const inputs = form.elements;

  // The structured data to send to the browser.
  const inputValue = {
    tailored_verdict_type: inputs.tailored_verdict_type.value,
  };

  sendWithPromise('setTailoredVerdictOverride', inputValue)
      .then(displayTailoredVerdictOverride);
}

function clearTailoredVerdictOverride(e: Event) {
  // Don't navigate
  e.preventDefault();

  const form = $<TailoredVerdictOverrideForm>('tailored-verdict-override-form');
  assert(form);
  form.reset();

  sendWithPromise('clearTailoredVerdictOverride')
      .then(displayTailoredVerdictOverride);
}

function showTab(tabId: string) {
  const tabs = document.querySelectorAll('div[slot=\'tab\']');
  const index = Array.from(tabs).findIndex(t => t.id === tabId);
  if (index !== -1) {
    const tabbox = document.querySelector('cr-tab-box');
    assert(tabbox);
    tabbox.setAttribute('selected-index', index.toString());
  }
}

document.addEventListener('DOMContentLoaded', initialize);
