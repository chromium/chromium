// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './inspect.mojom-webui.js';

const callbackRouter = new PageCallbackRouter();
const handler = new PageHandlerRemote();

PageHandlerFactory.getRemote().createPageHandler(
    callbackRouter.$.bindNewPipeAndPassRemote(),
    handler.$.bindNewPipeAndPassReceiver());


/**
 * Converts a frameID into a string suitable for use as an element's identifier.
 * @param frameId The frame ID to convert into an identifier.
 * @return A string compatible for use as an element identifier.
 */
function getFrameIdString(frameId: string): string {
  return 'frame-id-' + frameId;
}

/**
 * Creates an element to display a frame's location and logs.
 * @param frameId The frame ID of the frame which this element will
 *                  represent.
 * @return An element which represents the frame with |frameId|.
 */
function createFrameElement(frameId: string): HTMLElement {
  const frame = document.createElement('div');
  frame.id = getFrameIdString(frameId);
  frame.className = 'frame';

  const locationDiv = document.createElement('div');
  locationDiv.className = 'location';
  frame.appendChild(locationDiv);

  frame.appendChild(document.createElement('hr'));

  const logs = document.createElement('div');
  logs.className = 'logs';
  frame.appendChild(logs);

  const childFramesLabel = document.createElement('div');
  childFramesLabel.className = 'child-frames-label';
  childFramesLabel.appendChild(document.createTextNode('Child frames: '));
  frame.appendChild(childFramesLabel);

  const childFrames = document.createElement('div');
  childFrames.className = 'child-frames';
  frame.appendChild(childFrames);

  return frame;
}

/**
 * Adds |message| to the UI organized by the |mainFrameId| and |frameId|.
 * @param mainFrameId The frame ID for the main frame of the webpage
 *                  which sent the message.
 * @param frameId The frame ID of the frame which sent the message.
 * @param frameLocation The URL of the frame which sent the message.
 * @param level The log level associated with the message.
 * @param message The message text.
 */
function logMessageReceived(
    mainFrameId: string, frameId: string, frameLocation: string, level: string,
    message: string) {
  let tab = document.getElementById(getFrameIdString(mainFrameId));
  if (!tab) {
    tab = createFrameElement(mainFrameId);
    tab.classList.add('tab');
    document.getElementById('tabs')!.appendChild(tab);
  }

  let frame: HTMLElement;
  if (mainFrameId === frameId) {
    frame = tab;
  } else {
    tab.querySelector<HTMLElement>('.child-frames-label')!.style.display =
        'inline';
    const childFrames = tab.querySelector('.child-frames')!;
    let childFrame =
        childFrames.querySelector<HTMLElement>('#' + getFrameIdString(frameId));
    if (!childFrame) {
      childFrame = createFrameElement(frameId);
      childFrames.appendChild(childFrame);
    }
    frame = childFrame;
  }

  const locationDiv = frame.querySelector<HTMLElement>('.location')!;
  locationDiv.innerHTML = '';
  locationDiv.appendChild(document.createTextNode(frameLocation));

  const log = document.createElement('div');
  log.className = 'log';

  const logLevel = document.createElement('span');
  logLevel.className = 'log-level';
  if (level === 'warn') {
    logLevel.classList.add('warning');
  } else if (level === 'error') {
    logLevel.classList.add('error');
  }
  logLevel.appendChild(document.createTextNode(level.toUpperCase()));
  log.appendChild(logLevel);

  log.appendChild(document.createTextNode(message));

  frame.querySelector('.logs')!.appendChild(log);
}

callbackRouter.logMessageReceived.addListener(
    (mainFrameId: string, frameId: string, frameLocation: string, level: string,
     message: string) => {
      logMessageReceived(mainFrameId, frameId, frameLocation, level, message);
    });

/**
 * Notify the application to start collecting console logs.
 */
function startLogging() {
  handler.setLoggingEnabled(true);
  document.getElementById('start-logging')!.hidden = true;
  document.getElementById('stop-logging')!.hidden = false;
}

/**
 * Notify the application to stop collecting console logs.
 */
function stopLogging() {
  handler.setLoggingEnabled(false);
  document.getElementById('tabs')!.innerHTML = '';
  document.getElementById('start-logging')!.hidden = false;
  document.getElementById('stop-logging')!.hidden = true;
}

document.addEventListener('DOMContentLoaded', function() {
  document.getElementById('start-logging')!.onclick = startLogging;
  document.getElementById('stop-logging')!.onclick = stopLogging;
});
