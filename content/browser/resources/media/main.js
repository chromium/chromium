// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {objectForEach} from './util.js';

let manager = null;

window.media = window.media || {};

/**
 * Users of |media| must call initialize prior to calling other methods.
 */
export function initialize(theManager) {
  manager = theManager;

  // |chrome| is not defined during tests.
  if (window.chrome && window.chrome.send) {
    chrome.send('getEverything');
  }
}

// Exporting initialize on |window| for tests.
window.initialize = initialize;

// Adding the functions below on |window| since they are called from C++.
window.media.updateGeneralAudioInformation = function(audioInfo) {
  manager.updateGeneralAudioInformation(audioInfo);
};

window.media.onReceiveAudioStreamData = function(audioStreamData) {
  for (const component in audioStreamData) {
    window.media.updateAudioComponent(audioStreamData[component]);
  }
};

window.media.onReceiveVideoCaptureCapabilities = function(
    videoCaptureCapabilities) {
  manager.updateVideoCaptureCapabilities(videoCaptureCapabilities);
};

window.media.onReceiveAudioFocusState = function(audioFocusState) {
  if (!audioFocusState) {
    return;
  }

  manager.updateAudioFocusSessions(audioFocusState.sessions);
};

window.media.updateRegisteredCdms = function(cdms) {
  if (!cdms) {
    return;
  }

  manager.updateRegisteredCdms(cdms);
};

window.media.updateAudioComponent = function(component) {
  const uniqueComponentId = component.owner_id + ':' + component.component_id;
  switch (component.status) {
    case 'closed':
      manager.removeAudioComponent(component.component_type, uniqueComponentId);
      break;
    default:
      manager.updateAudioComponent(
          component.component_type, uniqueComponentId, component);
      break;
  }
};

window.media.onPlayerOpen = function(id, timestamp) {
  manager.addPlayer(id, timestamp);
};

window.media.onMediaEvent = function(event) {
  const source = event.renderer + ':' + event.player;

  // Although this gets called on every event, there is nothing we can do
  // because there is no onOpen event.
  media.onPlayerOpen(source);
  manager.updatePlayerInfoNoRecord(
      source, event.ticksMillis, 'render_id', event.renderer);
  manager.updatePlayerInfoNoRecord(
      source, event.ticksMillis, 'player_id', event.player);

  let propertyCount = 0;
  objectForEach(event.params, function(value, key) {
    key = key.trim();
    manager.updatePlayerInfo(source, event.ticksMillis, key, value);
    propertyCount += 1;
  });

  if (propertyCount === 0) {
    manager.updatePlayerInfo(source, event.ticksMillis, 'event', event.type);
  }
};
