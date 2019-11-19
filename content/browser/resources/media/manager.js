// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps track of all the existing PlayerInfo and
 * audio stream objects and is the entry-point for messages from the backend.
 *
 * The events captured by Manager (add, remove, update) are relayed
 * to the clientRenderer which it can choose to use to modify the UI.
 */
var Manager = (function() {
  'use strict';

  function Manager(clientRenderer) {
    this.players_ = {};
    this.audioInfo_ = {};
    this.audioComponents_ = [];
    this.clientRenderer_ = clientRenderer;

    var copyAllPlayerButton = $('copy-all-player-button');
    var copyAllAudioButton = $('copy-all-audio-button');
    var hidePlayersButton = $('hide-players-button');
    var devtoolsNoticeWindow = $('devtools-notice-window');

    // In tests we may not have these buttons.
    if (copyAllPlayerButton) {
      copyAllPlayerButton.onclick = function() {
        this.clientRenderer_.showClipboard(
            JSON.stringify(this.players_, null, 2));
      }.bind(this);
    }
    if (copyAllAudioButton) {
      copyAllAudioButton.onclick = function() {
        this.clientRenderer_.showClipboard(
            JSON.stringify(this.audioInfo_, null, 2) + '\n\n' +
            JSON.stringify(this.audioComponents_, null, 2));
      }.bind(this);
    }
    if (hidePlayersButton) {
      hidePlayersButton.onclick = this.hidePlayers_.bind(this);
    }
    if (devtoolsNoticeWindow) {
      devtoolsNoticeWindow.onclick = this.hideNoticeWindow_;
    }
  }

  Manager.prototype = {
    /**
     * Updates the audio focus state.
     * @param sessions A list of media sessions that contain the current state.
     */
    updateAudioFocusSessions: function(sessions) {
      this.clientRenderer_.audioFocusSessionUpdated(sessions);
    },

    /**
     * Updates the general audio information.
     * @param audioInfo The map of information.
     */
    updateGeneralAudioInformation: function(audioInfo) {
      this.audioInfo_ = audioInfo;
      this.clientRenderer_.generalAudioInformationSet(this.audioInfo_);
    },

    /**
     * Updates an audio-component.
     * @param componentType Integer AudioComponent enum value; must match values
     * from the AudioLogFactory::AudioComponent enum.
     * @param componentId The unique-id of the audio-component.
     * @param componentData The actual component data dictionary.
     */
    updateAudioComponent: function(componentType, componentId, componentData) {
      if (!(componentType in this.audioComponents_)) {
        this.audioComponents_[componentType] = {};
      }
      if (!(componentId in this.audioComponents_[componentType])) {
        this.audioComponents_[componentType][componentId] = componentData;
      } else {
        for (var key in componentData) {
          this.audioComponents_[componentType][componentId][key] =
              componentData[key];
        }
      }
      this.clientRenderer_.audioComponentAdded(
          componentType, this.audioComponents_[componentType]);
    },

    /**
     * Removes an audio-stream from the manager.
     * @param id The unique-id of the audio-stream.
     */
    removeAudioComponent: function(componentType, componentId) {
      if (!(componentType in this.audioComponents_) ||
          !(componentId in this.audioComponents_[componentType])) {
        return;
      }

      delete this.audioComponents_[componentType][componentId];
      this.clientRenderer_.audioComponentRemoved(
          componentType, this.audioComponents_[componentType]);
    },

    /**
     * Adds a player to the list of players to manage.
     */
    addPlayer: function(id) {
      if (this.players_[id]) {
        return;
      }
      // Make the PlayerProperty and add it to the mapping
      this.players_[id] = new PlayerInfo(id);
      this.clientRenderer_.playerAdded(this.players_, this.players_[id]);
    },

    /**
     * Attempts to remove a player from the UI.
     * @param id The ID of the player to remove.
     */
    removePlayer: function(id) {
      var playerRemoved = this.players_[id];
      delete this.players_[id];
      this.clientRenderer_.playerRemoved(this.players_, playerRemoved);
    },

    hidePlayers_: function() {
      util.object.forEach(this.players_, function(playerInfo, id) {
        this.removePlayer(id);
      }, this);
    },

    hideNoticeWindow_: function() {
      this.style.display = 'none';
    },

    updatePlayerInfoNoRecord: function(id, timestamp, key, value) {
      if (!this.players_[id]) {
        console.error('[updatePlayerInfo] Id ' + id + ' does not exist');
        return;
      }

      this.players_[id].addPropertyNoRecord(timestamp, key, value);
      this.clientRenderer_.playerUpdated(
          this.players_, this.players_[id], key, value);
    },

    /**
     *
     * @param id The unique ID that identifies the player to be updated.
     * @param timestamp The timestamp of when the change occured.  This
     * timestamp is *not* normalized.
     * @param key The name of the property to be added/changed.
     * @param value The value of the property.
     */
    updatePlayerInfo: function(id, timestamp, key, value) {
      if (!this.players_[id]) {
        console.error('[updatePlayerInfo] Id ' + id + ' does not exist');
        return;
      }

      this.players_[id].addProperty(timestamp, key, value);
      this.clientRenderer_.playerUpdated(
          this.players_, this.players_[id], key, value);
    },

    parseVideoCaptureFormat_: function(format) {
      /**
       * Example:
       *
       * format:
       *   "(160x120)@30.000fps, pixel format: PIXEL_FORMAT_I420, storage: CPU"
       *
       * formatDict:
       *   {'resolution':'1280x720', 'fps': '30.00', "storage: "CPU" }
       */
      var parts = format.split(', ');
      var formatDict = {};
      for (var i in parts) {
        var kv = parts[i].split(': ');
        if (kv.length == 2) {
          if (kv[0] == 'pixel format') {
            // The camera does not actually output I420,
            // so this info is misleading.
            continue;
          }
          formatDict[kv[0]] = kv[1];
        } else {
          kv = parts[i].split('@');
          if (kv.length == 2) {
            formatDict['resolution'] = kv[0].replace(/[)(]/g, '');
            // Round down the FPS to 2 decimals.
            formatDict['fps'] =
                parseFloat(kv[1].replace(/fps$/, '')).toFixed(2);
          }
        }
      }

      return formatDict;
    },

    updateVideoCaptureCapabilities: function(videoCaptureCapabilities) {
      // Parse the video formats to be structured for the table.
      for (var i in videoCaptureCapabilities) {
        for (var j in videoCaptureCapabilities[i]['formats']) {
          videoCaptureCapabilities[i]['formats'][j] =
              this.parseVideoCaptureFormat_(
                  videoCaptureCapabilities[i]['formats'][j]);
        }
      }

      // The keys of each device to be shown in order of appearance.
      var videoCaptureDeviceKeys = ['name', 'formats', 'captureApi', 'id'];

      this.clientRenderer_.redrawVideoCaptureCapabilities(
          videoCaptureCapabilities, videoCaptureDeviceKeys);
    }
  };

  return Manager;
}());
