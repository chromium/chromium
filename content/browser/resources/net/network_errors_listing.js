// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('errorCodes', function() {
  'use strict';

  /**
   * Generate the page content.
   * @param {Array.<Object>} errorCodes Error codes array consisting of a
   *    numerical error ID and error code string.
   */
  function listErrorCodes(errorCodes) {
    var errorPageUrl = 'chrome://network-error/';
    var errorCodesList = document.createElement('ul');
    for (var i = 0; i < errorCodes.length; i++) {
      var listEl = document.createElement('li');
      var errorCodeLinkEl = document.createElement('a');
      errorCodeLinkEl.href = errorPageUrl + errorCodes[i].errorId;
      errorCodeLinkEl.textContent =
          errorCodes[i].errorCode + ' (' + errorCodes[i].errorId + ')';
      listEl.appendChild(errorCodeLinkEl);
      errorCodesList.appendChild(listEl);
    }
    $('pages').appendChild(errorCodesList);
  }

  function initialize() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', 'network-error-data.json');
    xhr.addEventListener('load', function(e) {
      if (xhr.status === 200) {
        try {
          var data = JSON.parse(xhr.responseText);
          listErrorCodes(data['errorCodes']);
        } catch (e) {
          $('pages').innerText = 'Could not parse the error codes data. ' +
              'Try reloading the page.';
        }
      }
    });
    xhr.send();
  }

  return {initialize: initialize};
});

document.addEventListener('DOMContentLoaded', errorCodes.initialize);