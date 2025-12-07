// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Typescript for user_actions.html, served from chrome://user-actions/
 * This is used to debug user actions recording. It displays a live
 * stream of all user action events that occur in chromium while the
 * chrome://user-actions/ page is open.
 */

import {addWebUiListener} from '//resources/js/cr.js';
import {getRequiredElement} from '//resources/js/util.js';

/**
 * Appends a row to the output table listing the user action observed
 * and the current timestamp.
 * @param userAction the name of the user action observed.
 */
function observeUserAction(userAction: string): void {
  const table = getRequiredElement('user-actions-table');
  const tr = document.createElement('tr');
  let td = document.createElement('td');
  td.textContent = userAction;
  tr.appendChild(td);
  td = document.createElement('td');
  td.textContent = (Date.now() / 1000).toString();  // in seconds since epoch
  tr.appendChild(td);
  table.appendChild(tr);
}

document.addEventListener('DOMContentLoaded', function() {
  addWebUiListener('user-action', observeUserAction);
  // <if expr="not is_ios">
  chrome.send('pageLoaded');
  // </if>
});
