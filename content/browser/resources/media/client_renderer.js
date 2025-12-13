// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {millisecondsToString} from './util.js';
import '/strings.m.js';


/**
 * CSS classes added / removed in JS to trigger styling changes.
 * @enum {string}
 */
const ClientRendererCss = {
  NO_PLAYERS_SELECTED: 'no-players-selected',
  NO_COMPONENTS_SELECTED: 'no-components-selected',
  SELECTABLE_BUTTON: 'selectable-button',
  ERRORED_PLAYER: 'errored-player',
  ENDED_PLAYER: 'ended-player',
  ACTIVE_PLAYER: 'active-player',
};

function removeChildren(element) {
  while (element.hasChildNodes()) {
    element.removeChild(element.lastChild);
  }
}

function createSelectableButton(
    id, groupName, buttonLabel, selectCb, playerState) {
  // For CSS styling.
  const radioButton = document.createElement('input');
  radioButton.classList.add(ClientRendererCss.SELECTABLE_BUTTON);
  radioButton.type = 'radio';
  radioButton.id = id;
  radioButton.name = groupName;

  buttonLabel.classList.add(ClientRendererCss.SELECTABLE_BUTTON);
  if (playerState === 'errored') {
    buttonLabel.classList.add(ClientRendererCss.ERRORED_PLAYER);
  } else if (playerState === 'ended') {
    buttonLabel.classList.add(ClientRendererCss.ENDED_PLAYER);
  } else {
    buttonLabel.classList.add(ClientRendererCss.ACTIVE_PLAYER);
  }
  buttonLabel.setAttribute('for', radioButton.id);

  const fragment = document.createDocumentFragment();
  fragment.appendChild(radioButton);
  fragment.appendChild(buttonLabel);

  // Listen to 'change' rather than 'click' to keep styling in sync with
  // button behavior.
  radioButton.addEventListener('change', selectCb);

  return fragment;
}

function selectSelectableButton(id) {
  // |id| is usually not a valid selector for querySelector so we cannot use $
  // here.
  const element = document.getElementById(id);
  if (!element) {
    console.error('failed to select button with id: ' + id);
    return;
  }

  element.checked = true;
}

function downloadLog(text) {
  const file = new Blob([text], {type: 'text/plain'});
  const a = document.createElement('a');
  a.href = URL.createObjectURL(file);
  a.download = 'media-internals.txt';
  a.click();
}

export class ClientRenderer {
  constructor() {
    this.playerListElement = $('player-list');
    const audioTableElement = $('audio-property-table');
    if (audioTableElement) {
      this.audioPropertiesTable = audioTableElement.querySelector('tbody');
    }
    const playerTableElement = $('player-property-table');
    if (playerTableElement) {
      this.playerPropertiesTable = playerTableElement.querySelector('tbody');
    }
    const logElement = $('log');
    if (logElement) {
      this.logTable = logElement.querySelector('tbody');
    }
    this.graphElement = $('graphs');
    this.audioPropertyName = $('audio-property-name');
    this.audioFocusSessionListElement_ = $('audio-focus-session-list');
    this.cdmListElement_ =  $('cdm-list');
    const generalAudioInformationTableElement = $('general-audio-info-table');
    if (generalAudioInformationTableElement) {
      this.generalAudioInformationTable =
          generalAudioInformationTableElement.querySelector('tbody');
    }

    this.players = null;
    this.selectedPlayer = null;
    this.selectedAudioComponentType = null;
    this.selectedAudioComponentId = null;
    this.selectedAudioCompontentData = null;

    this.selectedPlayerLogIndex = 0;

    this.filterFunction = function() {
      return true;
    };
    this.filterText = $('filter-text');
    if (this.filterText) {
      this.filterText.onkeyup = this.onTextChange_.bind(this);
    }

    this.copyLogButton = $('copy-log-button');
    if (this.copyLogButton) {
      this.copyLogButton.onclick = this.copyLog_.bind(this);
    }

    this.saveLogButton = $('save-log-button');
    if (this.saveLogButton) {
      this.saveLogButton.onclick = this.saveLog_.bind(this);
    }

    this.closePlayerViewButton = $('close-player-view-button');
    if (this.closePlayerViewButton) {
      this.closePlayerViewButton.onclick = () => {
        $('main-container').classList.remove('mobile-player-view-active');
        document.body.classList.add(ClientRendererCss.NO_PLAYERS_SELECTED);
        if (this.selectedPlayer) {
          const element = this.playerListElement.querySelector(
              `.tree-item[data-id="${this.selectedPlayer.id}"]`);
          if (element) {
            element.classList.remove('selected');
          }
          this.selectedPlayer = null;
          const titleElement = $('player-details-title');
          if (titleElement) {
            titleElement.textContent = 'Player Properties';
            titleElement.title = '';
          }
        }
      };
    }

    this.hiddenKeys = ['component_id', 'component_type', 'owner_id'];
    this.revision = loadTimeData.getString('revision');

    document.body.classList.add(ClientRendererCss.NO_PLAYERS_SELECTED);
  }

  /**
   * Called to set general audio information.
   * @param audioInfo The map of information.
   */
  generalAudioInformationSet(audioInfo) {
    this.drawProperties_(audioInfo, this.generalAudioInformationTable);
  }

  /**
   * Called when an audio component is added to the collection.
   * @param componentType Integer AudioComponent enum value; must match values
   * from the AudioLogFactory::AudioComponent enum.
   * @param components The entire map of components (name -> dict).
   */
  audioComponentAdded(componentType, components) {
    this.redrawAudioComponentList_(componentType, components);

    // Redraw the component if it's currently selected.
    if (this.selectedAudioComponentType === componentType &&
        this.selectedAudioComponentId &&
        this.selectedAudioComponentId in components) {
      // TODO(chcunningham): This path is used both for adding and updating
      // the components. Split this up to have a separate update method.
      // At present, this selectAudioComponent call is key to *updating* the
      // the property table for existing audio components.
      this.selectAudioComponent_(
          componentType, this.selectedAudioComponentId,
          components[this.selectedAudioComponentId]);
    }
  }

  /**
   * Called when the list of audio focus sessions has changed.
   * @param sessions A list of media sessions that contain the current state.
   */
  audioFocusSessionUpdated(sessions) {
    removeChildren(this.audioFocusSessionListElement_);

    sessions.forEach(session => {
      this.audioFocusSessionListElement_.appendChild(
          this.createAudioFocusSessionRow_(session));
    });
  }

  /**
   * Called when the list of CDM info has changed.
   * @param sessions A list of CDM info that contain the current state.
   */
  updateRegisteredCdms(cdms) {
    removeChildren(this.cdmListElement_);

    cdms.forEach(cdm => {
      this.cdmListElement_.appendChild(this.createCdmRow_(cdm));
    });
  }

  /**
   * Called when an audio component is removed from the collection.
   * @param componentType Integer AudioComponent enum value; must match values
   * from the AudioLogFactory::AudioComponent enum.
   * @param components The entire map of components (name -> dict).
   */
  audioComponentRemoved(componentType, components) {
    this.redrawAudioComponentList_(componentType, components);
  }

  /**
   * Called when a player is added to the collection.
   * @param players The entire map of id -> player.
   * @param player_added The player that is added.
   */
  playerAdded(players, playerAdded) {
    this.redrawPlayerList_(players);
  }

  /**
   * Called when a player is removed from the collection.
   * @param players The entire map of id -> player.
   * @param playerRemoved The player that was removed.
   */
  playerRemoved(players, playerRemoved) {
    if (playerRemoved === this.selectedPlayer) {
      removeChildren(this.playerPropertiesTable);
      removeChildren(this.logTable);
      removeChildren(this.graphElement);
      document.body.classList.add(ClientRendererCss.NO_PLAYERS_SELECTED);
      this.selectedPlayer = null;
      const titleElement = $('player-details-title');
      if (titleElement) {
        titleElement.textContent = 'Player Properties';
        titleElement.title = '';
      }
    }
    this.redrawPlayerList_(players);
  }

  /**
   * Called when a property on a player is changed.
   * @param players The entire map of id -> player.
   * @param player The player that had its property changed.
   * @param key The name of the property that was changed.
   * @param value The new value of the property.
   */
  playerUpdated(players, player, key, value) {
    if (player === this.selectedPlayer) {
      this.drawProperties_(player.properties, this.playerPropertiesTable);
      this.drawLog_();
    }
    if (key === 'error') {
      player.playerState = 'errored';
    } else if (
        key === 'event' && value === 'kWebMediaPlayerDestroyed' &&
        player.playerState !== 'errored') {
      player.playerState = 'ended';
    }
    if ([
          'url',
          'frame_url',
          'frame_title',
          'audio_codec_name',
          'video_codec_name',
          'width',
          'height',
          'event',
          'error',
        ].includes(key)) {
      this.redrawPlayerList_(players);
    }
  }

  createVideoCaptureFormatTable(formats) {
    if (!formats || formats.length === 0) {
      return document.createTextNode('No formats');
    }

    const table = document.createElement('table');
    const thead = document.createElement('thead');
    const theadRow = document.createElement('tr');
    for (const key in formats[0]) {
      const th = document.createElement('th');
      th.appendChild(document.createTextNode(key));
      theadRow.appendChild(th);
    }
    thead.appendChild(theadRow);
    table.appendChild(thead);
    const tbody = document.createElement('tbody');
    for (let i = 0; i < formats.length; ++i) {
      const tr = document.createElement('tr');
      for (const key in formats[i]) {
        const td = document.createElement('td');
        td.appendChild(document.createTextNode(formats[i][key]));
        tr.appendChild(td);
      }
      tbody.appendChild(tr);
    }
    table.appendChild(tbody);
    table.classList.add('video-capture-formats-table');
    return table;
  }

  redrawVideoCaptureCapabilities(videoCaptureCapabilities, keys) {
    const copyButtonElement = $('video-capture-capabilities-copy-button');
    copyButtonElement.onclick = function() {
      this.renderClipboard(JSON.stringify(videoCaptureCapabilities, null, 2));
    }.bind(this);

    const videoTableBodyElement = $('video-capture-capabilities-tbody');
    removeChildren(videoTableBodyElement);

    for (const component in videoCaptureCapabilities) {
      const tableRow = document.createElement('tr');
      const device = videoCaptureCapabilities[component];
      for (const i in keys) {
        const value = device[keys[i]];
        const tableCell = document.createElement('td');
        let cellElement;
        if ((typeof value) === (typeof[])) {
          cellElement = this.createVideoCaptureFormatTable(value);
        } else {
          cellElement = document.createTextNode(
              ((typeof value) === 'undefined') ? 'n/a' : value);
        }
        tableCell.appendChild(cellElement);
        tableRow.appendChild(tableCell);
      }
      videoTableBodyElement.appendChild(tableRow);
    }
  }

  getAudioComponentName_(componentType, id) {
    let baseName;
    switch (componentType) {
      case 0:
      case 1:
        baseName = 'Controller';
        break;
      case 2:
        baseName = 'Stream';
        break;
      default:
        baseName = 'UnknownType';
        console.error('Unrecognized component type: ' + componentType);
        break;
    }
    return baseName + ' ' + id;
  }

  getListElementForAudioComponent_(componentType) {
    let listElement;
    switch (componentType) {
      case 0:
        listElement = $('audio-input-controller-list');
        break;
      case 1:
        listElement = $('audio-output-controller-list');
        break;
      case 2:
        listElement = $('audio-output-stream-list');
        break;
      default:
        console.error('Unrecognized component type: ' + componentType);
        listElement = null;
        break;
    }
    return listElement;
  }

  redrawAudioComponentList_(componentType, components) {
    const listElement = this.getListElementForAudioComponent_(componentType);
    if (!listElement) {
      console.error(
          'Failed to find list element for component type: ' + componentType);
      return;
    }

    const fragment = document.createDocumentFragment();
    for (const id in components) {
      const component = components[id];

      const treeItem = document.createElement('div');
      treeItem.classList.add('tree-item');
      treeItem.dataset.id = id;
      treeItem.classList.add(ClientRendererCss.ACTIVE_PLAYER);

      const treeItemHeader = document.createElement('div');
      treeItemHeader.classList.add('tree-item-header');
      treeItemHeader.textContent =
          this.getAudioComponentName_(componentType, id);
      treeItem.appendChild(treeItemHeader);

      const children = document.createElement('div');
      children.classList.add('tree-item-children');
      treeItem.appendChild(children);

      treeItemHeader.addEventListener('click', (e) => {
        treeItem.classList.toggle('expanded');
        this.selectAudioComponent_(componentType, id, component);
      });

      fragment.appendChild(treeItem);
    }
    removeChildren(listElement);
    listElement.appendChild(fragment);

    if (this.selectedAudioComponentType &&
        this.selectedAudioComponentType === componentType &&
        this.selectedAudioComponentId in components) {
      // Re-select the selected component since the button was just recreated.
      const element = listElement.querySelector(
          `.tree-item[data-id="${this.selectedAudioComponentId}"]`);
      if (element) {
        element.classList.add('selected');
      }
    }
  }

  selectAudioComponent_(componentType, componentId, componentData) {
    const audioWrapper = $('audio-component-list-wrapper');
    if (audioWrapper) {
      const previouslySelected =
          audioWrapper.querySelector('.tree-item.selected');
      if (previouslySelected) {
        previouslySelected.classList.remove('selected');
      }
    }

    const listElement = this.getListElementForAudioComponent_(componentType);
    if (listElement) {
      const element =
          listElement.querySelector(`.tree-item[data-id="${componentId}"]`);
      if (element) {
        element.classList.add('selected');
      }
    }

    this.selectedAudioComponentType = componentType;
    this.selectedAudioComponentId = componentId;
    this.selectedAudioCompontentData = componentData;
    this.drawProperties_(componentData, this.audioPropertiesTable);

    removeChildren(this.audioPropertyName);
    this.audioPropertyName.appendChild(document.createTextNode(
        this.getAudioComponentName_(componentType, componentId)));
  }

  redrawPlayerList_(players) {
    this.players = players;

    const fragment = document.createDocumentFragment();
    for (const id in players) {
      const player = players[id];
      const p = player.properties;

      const treeItem = document.createElement('div');
      treeItem.classList.add('tree-item');
      if (player.playerState === 'errored') {
        treeItem.classList.add(ClientRendererCss.ERRORED_PLAYER);
      } else if (player.playerState === 'ended') {
        treeItem.classList.add(ClientRendererCss.ENDED_PLAYER);
      } else {
        treeItem.classList.add(ClientRendererCss.ACTIVE_PLAYER);
      }
      treeItem.dataset.id = id;

      const treeItemHeader = document.createElement('div');
      treeItemHeader.classList.add('tree-item-header');
      treeItemHeader.classList.add('selectable-button');

      const playerName = document.createElement('div');
      playerName.classList.add('player-name');
      const url = p.url || 'Player ' + player.id;
      if (url.length > 64) {
        playerName.textContent = url.substring(0, 61) + '...';
      } else {
        playerName.textContent = url;
      }
      playerName.title = url;
      treeItemHeader.appendChild(playerName);

      let lastEvent = '';
      for (let i = player.allEvents.length - 1; i >= 0; i--) {
        if (player.allEvents[i].key === 'event') {
          lastEvent = player.allEvents[i].value;
          break;
        }
      }

      if (lastEvent) {
        const playerFrame = document.createElement('div');
        playerFrame.classList.add('player-frame');
        playerFrame.textContent =
            lastEvent === 'kWebMediaPlayerDestroyed' ? 'Destroyed' : lastEvent;
        treeItemHeader.appendChild(playerFrame);
      }
      treeItem.appendChild(treeItemHeader);

      const children = document.createElement('div');
      children.classList.add('tree-item-children');
      treeItem.appendChild(children);

      treeItemHeader.addEventListener('click', (e) => {
        treeItem.classList.toggle('expanded');
        this.selectPlayer_(player);
      });

      fragment.appendChild(treeItem);
    }
    removeChildren(this.playerListElement);
    this.playerListElement.appendChild(fragment);

    if (this.selectedPlayer && this.selectedPlayer.id in players) {
      // Re-select the selected player since the button was just recreated.
      const element = this.playerListElement.querySelector(
          `.tree-item[data-id="${this.selectedPlayer.id}"]`);
      if (element) {
        element.classList.add('selected');
      }
    }
  }

  selectPlayer_(player) {
    if (window.innerWidth <= 768) {
      $('main-container').classList.add('mobile-player-view-active');
    }

    document.body.classList.remove(ClientRendererCss.NO_PLAYERS_SELECTED);

    const previouslySelected =
        this.playerListElement.querySelector('.tree-item.selected');
    if (previouslySelected) {
      previouslySelected.classList.remove('selected');
    }

    const element = this.playerListElement.querySelector(
        `.tree-item[data-id="${player.id}"]`);
    if (element) {
      element.classList.add('selected');
    }

    this.selectedPlayer = player;
    this.selectedPlayerLogIndex = 0;
    this.selectedAudioComponentType = null;
    this.selectedAudioComponentId = null;
    this.selectedAudioCompontentData = null;
    this.drawProperties_(player.properties, this.playerPropertiesTable);

    removeChildren(this.logTable);
    removeChildren(this.graphElement);
    this.drawLog_();

    const titleElement = $('player-details-title');
    if (titleElement) {
      const playerName = player.properties.url || 'Player ' + player.id;
      titleElement.textContent = playerName;
      titleElement.title = playerName;
    }
  }

  drawProperties_(propertyMap, propertiesTable) {
    removeChildren(propertiesTable);
    const sortedKeys = Object.keys(propertyMap).sort();
    for (let i = 0; i < sortedKeys.length; ++i) {
      const key = sortedKeys[i];
      if (this.hiddenKeys.indexOf(key) >= 0) {
        continue;
      }

      const value = propertyMap[key];
      const row = propertiesTable.insertRow(-1);
      const keyCell = row.insertCell(-1);
      const valueCell = row.insertCell(-1);

      keyCell.appendChild(document.createTextNode(key));
      valueCell.appendChild(this.createValueCellContent_(key, value));
    }
  }

  applyCodeSearchLinkage_(status_obj) {
    if (status_obj.hasOwnProperty('stack')) {
      status_obj['stack'] = status_obj['stack'].map(e => {
        if (typeof(e) === 'string') return e;
        return '~{' + e['file'] + '%' + e['line'] + '}~';
      });
    }
    if (status_obj.hasOwnProperty('cause')) {
      status_obj['cause'] = this.applyCodeSearchLinkage_(status_obj['cause']);
    }
    return status_obj;
  }

  createValueCellContent_(key, value) {
    // This is a bit of a hack, but it's the only way to get the stack trace
    // to link to the code search.
    const urlPrefix = 'https://source.chromium.org/chromium/chromium/src/+/main:';

    const re = new RegExp('~{([^%]*)%([0-9]+)}~', 'g');
    try {
      if (key === 'kHlsBufferedRanges') {
        return document.createTextNode(JSON.stringify(value));
      }
      const pre = document.createElement('pre');
      const text = JSON.stringify(this.applyCodeSearchLinkage_(value), null, 2);
      let lastIndex = 0;
      for (const match of text.matchAll(re)) {
        if (match.index > lastIndex) {
          pre.appendChild(
              document.createTextNode(text.substring(lastIndex, match.index)));
        }
        const a = document.createElement('a');
        a.href = urlPrefix + match[1] + ';l=' + match[2];

        // Building locally gives a commit hash of 80 zeros separated in the
        // middle by a dash.
        if (!this.revision.startsWith('0000000')) {
          a.href += ';drc=' + this.revision;
        }

        a.textContent = match[1] + '#' + match[2];
        a.target = '_blank';
        a.rel = 'noopener';
        pre.appendChild(a);
        lastIndex = match.index + match[0].length;
      }
      if (lastIndex < text.length) {
        pre.appendChild(document.createTextNode(text.substring(lastIndex)));
      }
      return pre;
    } catch (e) {
      return document.createTextNode(JSON.stringify(value));
    }
  }

  appendEventToLog_(event) {
    if (this.filterFunction(event.key)) {
      const row = this.logTable.insertRow(-1);
      row.classList.add('log-entry');

      const timestampCell = row.insertCell(-1);
      timestampCell.classList.add('log-timestamp');
      timestampCell.textContent = millisecondsToString(event.time);

      const propertyCell = row.insertCell(-1);
      propertyCell.classList.add('log-property');
      propertyCell.textContent = event.key;

      const valueCell = row.insertCell(-1);
      valueCell.classList.add('log-value');
      valueCell.appendChild(
          this.createValueCellContent_(event.key, event.value));

      if (event.key.toLowerCase().includes('error')) {
        row.classList.add('log-error');
      } else if (event.key.toLowerCase().includes('warning')) {
        row.classList.add('log-warning');
      }
    }
  }

  drawLog_() {
    const toDraw =
        this.selectedPlayer.allEvents.slice(this.selectedPlayerLogIndex);
    toDraw.forEach(this.appendEventToLog_.bind(this));
    this.selectedPlayerLogIndex = this.selectedPlayer.allEvents.length;
  }

  saveLog_() {
    const strippedPlayers = [];
    for (const id in this.players) {
      const p = this.players[id];
      strippedPlayers.push({properties: p.properties, events: p.allEvents});
    }
    downloadLog(JSON.stringify(strippedPlayers, null, 2));
  }

  copyLog_() {
    if (!this.selectedPlayer) {
      return;
    }

    // Copy both properties and events for convenience since both are useful
    // in bug reports.
    const p = this.selectedPlayer;
    const playerLog = {properties: p.properties, events: p.allEvents};

    this.renderClipboard(JSON.stringify(playerLog, null, 2));
  }

  renderClipboard(string) {
    navigator.clipboard.writeText(string);
  }

  onTextChange_(event) {
    const text = this.filterText.value.toLowerCase();
    const parts = text.split(',')
                      .map(function(part) {
                        return part.trim();
                      })
                      .filter(function(part) {
                        return part.trim().length > 0;
                      });

    this.filterFunction = function(text) {
      text = text.toLowerCase();
      return parts.length === 0 || parts.some(function(part) {
        return text.indexOf(part) !== -1;
      });
    };

    if (this.selectedPlayer) {
      removeChildren(this.logTable);
      this.selectedPlayerLogIndex = 0;
      this.drawLog_();
    }
  }

  createAudioFocusSessionRow_(session) {
    const template = $('audio-focus-session-row');
    const span = template.content.querySelectorAll('span');
    span[0].textContent = session.name;
    span[1].textContent = session.owner;
    span[2].textContent = session.state;
    return document.importNode(template.content, true);
  }

  createCdmRow_(cdm) {
    const template = $('cdm-row');
    const clone = document.importNode(template.content, true);
    const tableBody = clone.querySelector('tbody');


    const addRow = (key, value) => {
      const row = tableBody.insertRow(-1);
      const keyCell = row.insertCell(-1);
      const valueCell = row.insertCell(-1);
      keyCell.textContent = key;
      if (typeof value === 'object') {
        const pre = document.createElement('pre');
        pre.textContent = JSON.stringify(value, null, 2);
        valueCell.appendChild(pre);
      } else {
        valueCell.textContent = value;
      }
    };

    addRow('Key System', cdm.key_system);
    addRow('Robustness', cdm.robustness);
    addRow('Name', cdm.name);
    addRow('Version', cdm.version);
    addRow('Path', cdm.path);
    addRow('Status', cdm.status);
    addRow('Capabilities', cdm.capability);

    return clone;
  }
}
