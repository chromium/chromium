// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Converts a frameID into a string suitable for use as an element's identifier.
 * @param {!string} frameId The frame ID to convert into an identifier.
 * @return {!string} A string compatible for use as an element identifier.
 */
function getFrameIdString_(frameId) {
  return 'fid-' + frameId;
}

/**
 * Creates an element to display a frame's location and logs.
 * @param {!string} frameId The frame ID of the frame which this element will
 *                  represent.
 * @return {!Object} An element which represents the frame with |frameId|.
 */
function createFrameElement_(frameId) {
  let frame = document.createElement('div');
  frame.id = getFrameIdString_(frameId);
  frame.className = 'frame';

  let locationDiv = document.createElement('div');
  locationDiv.className = 'location';
  frame.appendChild(locationDiv);

  frame.appendChild(document.createElement('hr'));

  let logs = document.createElement('div');
  logs.className = 'logs';
  frame.appendChild(logs);

  let childFramesLabel = document.createElement('div');
  childFramesLabel.className = 'child-frames-label';
  childFramesLabel.appendChild(document.createTextNode('Child frames: '));
  frame.appendChild(childFramesLabel);

  let childFrames = document.createElement('div');
  childFrames.className = 'child-frames';
  frame.appendChild(childFrames);

  return frame;
}

/**
 * Adds |message| to the UI organized by the |mainFrameId| and |frameId|.
 * @param {!string} mainFrameId The frame ID for the main frame of the webpage
 *                  which sent the message.
 * @param {!string} frameId The frame ID of the frame which sent the message.
 * @param {!string} frameLocation The URL of the frame which sent the message.
 * @param {!string} level The log level associated with the message.
 * @param {!string} message The message text.
 */
function logMessageReceived(
    mainFrameId, frameId, frameLocation, level, message) {
  let tab = $(getFrameIdString_(mainFrameId));
  if (!tab) {
    tab = createFrameElement_(mainFrameId);
    tab.classList.add('tab')
    $('tabs').appendChild(tab);
  }

  let frame;
  if (mainFrameId === frameId) {
    frame = tab;
  } else {
    tab.querySelector('.child-frames-label').style.display = 'inline';
    frame = tab.querySelector('.child-frames')
                .querySelector('#' + getFrameIdString_(frameId));
    if (!frame) {
      frame = createFrameElement_(frameId);
      tab.querySelector('.child-frames').appendChild(frame);
    }
  }

  let locationDiv = frame.querySelector('.location');
  locationDiv.innerHTML = '';
  locationDiv.appendChild(document.createTextNode(frameLocation));

  let log = document.createElement('div');
  log.className = 'log';

  let logLevel = document.createElement('span');
  logLevel.className = 'log-level';
  if (level === 'warn') {
    level = 'warning';
    logLevel.classList.add('warning');
  } else if (level === 'error') {
    logLevel.classList.add('error');
  }
  logLevel.appendChild(document.createTextNode(level.toUpperCase()));
  log.appendChild(logLevel);

  log.appendChild(document.createTextNode(message));

  frame.querySelector('.logs').appendChild(log);
}

/**
 * Removes the messages and UI associated with |mainFrameId| and all children.
 * @param {!string} mainFrameId The frame ID for the main frame of the webpage
 *                  which sent the message.
 */
function tabClosed(mainFrameId) {
  let tab = $(getFrameIdString_(mainFrameId));
  if (tab) {
    $('tabs').removeChild(tab);
  }
}

/**
 * Notify the application to start collecting console logs.
 */
function startLogging() {
  chrome.send('setLoggingEnabled', [true]);
  $('start-logging').hidden = true;
  $('stop-logging').hidden = false;
}

/**
 * Notify the application to stop collecting console logs.
 */
function stopLogging() {
  chrome.send('setLoggingEnabled', [false]);
  $('tabs').innerHTML = '';
  $('start-logging').hidden = false;
  $('stop-logging').hidden = true;
}

document.addEventListener('DOMContentLoaded', function() {
  $('start-logging').onclick = startLogging;
  $('stop-logging').onclick = stopLogging;

  // Expose |logMessageReceived| and |tabClosed| functions through global
  // namespace as they will be called from the native app.
  __gCrWeb.inspectWebUI = {};
  __gCrWeb.inspectWebUI.logMessageReceived = logMessageReceived;
  __gCrWeb.inspectWebUI.tabClosed = tabClosed;
});
