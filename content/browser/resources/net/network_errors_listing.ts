// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util.js';

/**
 * Generate the page content.
 * @param errorCodes Error codes array consisting of a numerical error ID and
 *     error code string.
 */
function listErrorCodes(errorCodes: NetworkError[]) {
  const errorPageUrl = 'chrome://network-error/';
  const errorCodesList = document.createElement('ul');
  for (const error of errorCodes) {
    const listEl = document.createElement('li');
    const errorCodeLinkEl = document.createElement('a');
    errorCodeLinkEl.href = errorPageUrl + error.errorId;
    errorCodeLinkEl.textContent = error.errorCode + ' (' + error.errorId + ')';
    listEl.appendChild(errorCodeLinkEl);
    errorCodesList.appendChild(listEl);
  }
  getRequiredElement('pages').appendChild(errorCodesList);
}

interface NetworkError {
  errorCode: string;
  errorId: number;
}

function initialize() {
  const xhr = new XMLHttpRequest();
  xhr.open('GET', 'network-error-data.json');
  xhr.addEventListener('load', function(_e) {
    if (xhr.status === 200) {
      try {
        const data =
            JSON.parse(xhr.responseText) as {errorCodes: NetworkError[]};
        listErrorCodes(data['errorCodes']);
      } catch (e) {
        getRequiredElement('pages').textContent =
            'Could not parse the error codes data. ' +
            'Try reloading the page.';
      }
    }
  });
  xhr.send();
}

document.addEventListener('DOMContentLoaded', initialize);
