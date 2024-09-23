// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

import {PlayerInfo} from './player_info.js';
import {objectForEach} from './util.js';

/**
 * @fileoverview Keeps track of all the existing PlayerInfo and
 * audio stream objects and is the entry-point for messages from the backend.
 *
 * The events captured by Manager (add, remove, update) are relayed
 * to the clientRenderer which it can choose to use to modify the UI.
 */
export class Manager {
  constructor(clientRenderer) {
    this.players_ = {};
    this.audioInfo_ = {};
    this.audioComponents_ = [];
    this.clientRenderer_ = clientRenderer;

    const copyAllPlayerButton = $('copy-all-player-button');
    const copyAllAudioButton = $('copy-all-audio-button');
    const hidePlayersButton = $('hide-players-button');

    // In tests we may not have these buttons.
    if (copyAllPlayerButton) {
      copyAllPlayerButton.onclick = function() {
        this.clientRenderer_.renderClipboard(
            JSON.stringify(this.players_, null, 2));
      }.bind(this);
    }
    if (copyAllAudioButton) {
      copyAllAudioButton.onclick = function() {
        this.clientRenderer_.renderClipboard(
            JSON.stringify(this.audioInfo_, null, 2) + '\n\n' +
            JSON.stringify(this.audioComponents_, null, 2));
      }.bind(this);
    }
    if (hidePlayersButton) {
      hidePlayersButton.onclick = this.hidePlayers_.bind(this);
    }
  }

  /**
   * Updates the audio focus state.
   * @param sessions A list of media sessions that contain the current state.
   */
  updateAudioFocusSessions(sessions) {
    this.clientRenderer_.audioFocusSessionUpdated(sessions);
  }

  /**
   * Updates the registered CDM list.
   * @param cdms A list of registered Content Decryption Modules.
   */
  updateRegisteredCdms(cdms) {
    this.clientRenderer_.updateRegisteredCdms(cdms);
  }

  /**
   * Updates the general audio information.
   * @param audioInfo The map of information.
   */
  updateGeneralAudioInformation(audioInfo) {
    this.audioInfo_ = audioInfo;
    this.clientRenderer_.generalAudioInformationSet(this.audioInfo_);
  }

  /**
   * Updates an audio-component.
   * @param componentType Integer AudioComponent enum value; must match values
   * from the AudioLogFactory::AudioComponent enum.
   * @param componentId The unique-id of the audio-component.
   * @param componentData The actual component data dictionary.
   */
  updateAudioComponent(componentType, componentId, componentData) {
    if (!(componentType in this.audioComponents_)) {
      this.audioComponents_[componentType] = {};
    }
    if (!(componentId in this.audioComponents_[componentType])) {
      this.audioComponents_[componentType][componentId] = componentData;
    } else {
      for (const key in componentData) {
        this.audioComponents_[componentType][componentId][key] =
            componentData[key];
      }
    }
    this.clientRenderer_.audioComponentAdded(
        componentType, this.audioComponents_[componentType]);
  }

  /**
   * Removes an audio-stream from the manager.
   * @param id The unique-id of the audio-stream.
   */
  removeAudioComponent(componentType, componentId) {
    if (!(componentType in this.audioComponents_) ||
        !(componentId in this.audioComponents_[componentType])) {
      return;
    }

    delete this.audioComponents_[componentType][componentId];
    this.clientRenderer_.audioComponentRemoved(
        componentType, this.audioComponents_[componentType]);
  }

  /**
   * Adds a player to the list of players to manage.
   */
  addPlayer(id) {
    if (this.players_[id]) {
      return;
    }
    // Make the PlayerProperty and add it to the mapping
    this.players_[id] = new PlayerInfo(id);
    this.clientRenderer_.playerAdded(this.players_, this.players_[id]);
  }

  /**
   * Attempts to remove a player from the UI.
   * @param id The ID of the player to remove.
   */
  removePlayer(id) {
    const playerRemoved = this.players_[id];
    delete this.players_[id];
    this.clientRenderer_.playerRemoved(this.players_, playerRemoved);
  }

  hidePlayers_() {
    objectForEach(this.players_, function(playerInfo, id) {
      this.removePlayer(id);
    }, this);
  }

  updatePlayerInfoNoRecord(id, timestamp, key, value) {
    if (!this.players_[id]) {
      console.error('[updatePlayerInfo] Id ' + id + ' does not exist');
      return;
    }

    this.players_[id].addPropertyNoRecord(timestamp, key, value);
    this.clientRenderer_.playerUpdated(
        this.players_, this.players_[id], key, value);
  }

  /**
   *
   * @param id The unique ID that identifies the player to be updated.
   * @param timestamp The timestamp of when the change occurred.  This
   * timestamp is *not* normalized.
   * @param key The name of the property to be added/changed.
   * @param value The value of the property.
   */
  updatePlayerInfo(id, timestamp, key, value) {
    if (!this.players_[id]) {
      console.error('[updatePlayerInfo] Id ' + id + ' does not exist');
      return;
    }

    this.players_[id].addProperty(timestamp, key, value);
    this.clientRenderer_.playerUpdated(
        this.players_, this.players_[id], key, value);
  }

  parseVideoCaptureFormat_(format) {
    /**
     * Example:
     *
     * format:
     *   "(160x120)@30.000fps, pixel format: PIXEL_FORMAT_I420, storage: CPU"
     *
     * formatDict:
     *   {'resolution':'1280x720', 'fps': '30.00', "storage: "CPU" }
     */
    const parts = format.split(', ');
    const formatDict = {};
    for (const i in parts) {
      let kv = parts[i].split(': ');
      if (kv.length === 2) {
        if (kv[0] === 'pixel format') {
          // The camera does not actually output I420,
          // so this info is misleading.
          continue;
        }
        formatDict[kv[0]] = kv[1];
      } else {
        kv = parts[i].split('@');
        if (kv.length === 2) {
          formatDict['resolution'] = kv[0].replace(/[)(]/g, '');
          // Round down the FPS to 2 decimals.
          formatDict['fps'] = parseFloat(kv[1].replace(/fps$/, '')).toFixed(2);
        }
      }
    }

    return formatDict;
  }

  updateVideoCaptureCapabilities(videoCaptureCapabilities) {
    // Parse the video formats to be structured for the table.
    for (const i in videoCaptureCapabilities) {
      for (const j in videoCaptureCapabilities[i]['formats']) {
        videoCaptureCapabilities[i]['formats'][j] =
            this.parseVideoCaptureFormat_(
                videoCaptureCapabilities[i]['formats'][j]);
      }
      videoCaptureCapabilities[i]['controlSupport'] =
          videoCaptureCapabilities[i]['controlSupport'].join(' ') || 'N/A';
    }

    // The keys of each device to be shown in order of appearance.
    const videoCaptureDeviceKeys =
        ['name', 'formats', 'captureApi', 'controlSupport', 'id'];

    this.clientRenderer_.redrawVideoCaptureCapabilities(
        videoCaptureCapabilities, videoCaptureDeviceKeys);
  }
}

// Exporting the class on window for tests.
window.Manager = Manager;
