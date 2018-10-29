// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A global object that gets used by the C++ interface.
 */
var media = (function() {
  'use strict';

  var manager = null;

  /**
   * Users of |media| must call initialize prior to calling other methods.
   */
  media.initialize = function(theManager) {
    manager = theManager;
  };

  media.updateGeneralAudioInformation = function(audioInfo) {
    manager.updateGeneralAudioInformation(audioInfo);
  };

  media.onReceiveAudioStreamData = function(audioStreamData) {
    for (var component in audioStreamData) {
      media.updateAudioComponent(audioStreamData[component]);
    }
  };

  media.onReceiveVideoCaptureCapabilities = function(videoCaptureCapabilities) {
    manager.updateVideoCaptureCapabilities(videoCaptureCapabilities);
  };

  media.onReceiveAudioFocusState = function(audioFocusState) {
    if (!audioFocusState)
      return;

    manager.updateAudioFocusSessions(audioFocusState.sessions);
  };

  media.updateAudioComponent = function(component) {
    var uniqueComponentId = component.owner_id + ':' + component.component_id;
    switch (component.status) {
      case 'closed':
        manager.removeAudioComponent(
            component.component_type, uniqueComponentId);
        break;
      default:
        manager.updateAudioComponent(
            component.component_type, uniqueComponentId, component);
        break;
    }
  };

  media.onPlayerOpen = function(id, timestamp) {
    manager.addPlayer(id, timestamp);
  };

  media.onMediaEvent = function(event) {
    var source = event.renderer + ':' + event.player;

    // Although this gets called on every event, there is nothing we can do
    // because there is no onOpen event.
    media.onPlayerOpen(source);
    manager.updatePlayerInfoNoRecord(
        source, event.ticksMillis, 'render_id', event.renderer);
    manager.updatePlayerInfoNoRecord(
        source, event.ticksMillis, 'player_id', event.player);

    var propertyCount = 0;
    util.object.forEach(event.params, function(value, key) {
      key = key.trim();
      manager.updatePlayerInfo(source, event.ticksMillis, key, value);
      propertyCount += 1;
    });

    if (propertyCount === 0) {
      manager.updatePlayerInfo(source, event.ticksMillis, 'event', event.type);
    }
  };

  // |chrome| is not defined during tests.
  if (window.chrome && window.chrome.send) {
    chrome.send('getEverything');
  }
  return media;
}());
