// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';
const USER_MEDIA_TAB_ID = 'user-media-tab-id';

/**
 * A helper function for appending a child element to |parent|.
 *
 * @param {!Element} parent The parent element.
 * @param {string} tag The child element tag.
 * @param {string} text The textContent of the new DIV.
 * @return {!Element} the new DIV element.
 */
 function appendChildWithText(parent, tag, text) {
  const child = document.createElement(tag);
  child.textContent = text;
  parent.appendChild(child);
  return child;
}

export class UserMediaTable {
  /**
    * @param {Object} tabView the TabView object to add the user media tab to.
    */
  constructor(tabView) {
    this.tabView = tabView;
  }

  /**
   * Populate the tab view with a getUserMedia/getDisplayMedia tab.
   */
  createTab() {
    const container = this.tabView.addTab(USER_MEDIA_TAB_ID,
        'getUserMedia/getDisplayMedia');
    // Create the filter input field and label.
    appendChildWithText(container, 'label', 'Filter by origin including ');
    const input = document.createElement('input');
    input.size = 30;
    input.oninput = this.filterUserMedia.bind(this);
    container.appendChild(input);
  }

  /**
   * Apply a filter to the user media table.
   * @param event InputEvent from the filter input field.
   * @private
   */
  filterUserMedia(event) {
    const filter = event.target.value;
    const requests = $(USER_MEDIA_TAB_ID).childNodes;
    for (let i = 0; i < requests.length; ++i) {
      if (!requests[i]['data-origin']) {
        continue;
      }
      if (requests[i]['data-origin'].includes(filter)) {
        requests[i].style.display = 'block';
      } else {
        requests[i].style.display = 'none';
      }
    }
  }

  /**
   * Adds a getUserMedia/getDisplayMedia request.
   * @param {!Object} data The object containing rid {number}, pid {number},
   *     origin {string}, request_id {number}, request_type {string},
   *     audio {string}, video {string}.
   */
  addMedia(data) {
    if (!$(USER_MEDIA_TAB_ID)) {
      this.createTab();
    }

    const requestDiv = document.createElement('div');
    requestDiv.className = 'user-media-request-div-class';
    requestDiv.id = ['gum', data.rid, data.pid, data.request_id].join('-');
    requestDiv['data-rid'] = data.rid;
    requestDiv['data-origin'] = data.origin;
    // Insert new getUserMedia calls at the top.
    $(USER_MEDIA_TAB_ID).insertBefore(requestDiv,
      $(USER_MEDIA_TAB_ID).firstChild);

    appendChildWithText(requestDiv, 'div', 'Caller origin: ' + data.origin);
    appendChildWithText(requestDiv, 'div', 'Caller process id: ' + data.pid);

    const el = appendChildWithText(requestDiv, 'span',
      data.request_type + ' call');
    el.style.fontWeight = 'bold';
    appendChildWithText(el, 'div', 'Time: ' +
      (new Date(data.timestamp).toTimeString()))
      .style.fontWeight = 'normal';
    if (data.audio !== undefined) {
      appendChildWithText(el, 'div', 'Audio constraints: ' +
        (data.audio || 'true'))
        .style.fontWeight = 'normal';
    }
    if (data.video !== undefined) {
      appendChildWithText(el, 'div', 'Video constraints: ' +
        (data.video || 'true'))
        .style.fontWeight = 'normal';
    }
  }

  /**
   * Update a getUserMedia/getDisplayMedia request with a result or error.
   *
   * @param {!Object} data The object containing rid {number}, pid {number},
   *     request_id {number}, request_type {string}.
   *     For results there is also the
   *     stream_id {string}, audio_track_info {string} and
   *     video_track_info {string}.
   *     For errors the error {string} and
   *     error_message {string} fields are set.
   */
  updateMedia(data) {
    if (!$(USER_MEDIA_TAB_ID)) {
      this.createTab();
    }

    const requestDiv = document.getElementById(
      ['gum', data.rid, data.pid, data.request_id].join('-'));
    if (!requestDiv) {
      console.error('Could not update ' + data.request_type + ' request', data);
      return;
    }

    if (data.error) {
      const el = appendChildWithText(requestDiv, 'span', 'Error');
      el.style.fontWeight = 'bold';
      appendChildWithText(el, 'div', 'Time: ' +
        (new Date(data.timestamp).toTimeString()))
        .style.fontWeight = 'normal';
      appendChildWithText(el, 'div', 'Error: ' + data.error)
        .style.fontWeight = 'normal';
      appendChildWithText(el, 'div', 'Error message: ' + data.error_message)
        .style.fontWeight = 'normal';
      return;
    }

    const el = appendChildWithText(requestDiv, 'span',
        data.request_type + ' result');
    el.style.fontWeight = 'bold';
    appendChildWithText(el, 'div', 'Time: ' +
      (new Date(data.timestamp).toTimeString()))
      .style.fontWeight = 'normal';
    appendChildWithText(el, 'div', 'Stream id: ' + data.stream_id)
      .style.fontWeight = 'normal';
    if (data.audio_track_info) {
      appendChildWithText(el, 'div', 'Audio track: ' + data.audio_track_info)
          .style.fontWeight = 'normal';
    }
    if (data.video_track_info) {
      appendChildWithText(el, 'div', 'Video track: ' + data.video_track_info)
          .style.fontWeight = 'normal';
    }
  }

  /**
   * Removes the getUserMedia/getDisplayMedia requests from the specified |rid|.
   *
   * @param {!Object} data The object containing rid {number}, the render id.
   */
  removeMediaForRenderer(data) {
    const requests = $(USER_MEDIA_TAB_ID).childNodes;
    for (let i = 0; i < requests.length; ++i) {
      if (!requests[i]['data-origin']) {
        continue;
      }
      if (requests[i]['data-rid'] === data.rid) {
        $(USER_MEDIA_TAB_ID).removeChild(requests[i]);
      }
    }
    // Remove the tab when only the search field and its label are left.
    if ($(USER_MEDIA_TAB_ID).childNodes.length === 2) {
      this.tabView.removeTab(USER_MEDIA_TAB_ID);
    }
  }
}
