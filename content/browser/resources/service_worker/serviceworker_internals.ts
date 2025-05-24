// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {html, render} from 'chrome://resources/lit/v3_0/lit.rollup.js';

interface Options {
  debug_on_start: boolean;
}

interface Registration {
  active?: Version;
  ancestor_chain_bit: string;
  navigation_preload_enabled: boolean;
  navigation_preload_header_length: number;
  nonce: string;
  origin: string;
  registration_id: string;
  scope: string;
  storage_key: string;
  third_party_storage_partitioning_enabled: boolean;
  top_level_site: string;
  unregistered?: boolean;  // Augmented by the frontend.
  waiting?: Version;
}

interface Client {
  client_id: string;
  url: string;
}

interface Version {
  clients: Client[];
  devtools_agent_route_id: number;
  fetch_handler_existence: string;
  fetch_handler_type: string;
  process_host_id: number;
  process_id: number;
  router_rules?: string;
  running_status: string;
  script_url: string;
  status: string;
  thread_id: number;
  version_id: string;
}

interface PartitionsDataEntry {
  partitionPath: string;
  registrations: PartitionData;
}

interface PartitionData {
  liveRegistrations: Registration[];
  liveVersions: Version[];
  storedRegistrations: Registration[];
}

interface PartitionDataProcessed {
  partitionId: number;
  partitionPath: string;
  storedRegistrations: Registration[];
  unregisteredRegistrations: Registration[];
  unregisteredVersions: Version[];
}

function getVersionHtml(version: Version, partitionId: number) {
  // clang-format off
  return html`
    <div class="serviceworker-version">
      <div class="serviceworker-status">
        <span>Installation Status:</span>
        <span class="value">${version.status}</span>
      </div>
      <div class="serviceworker-running-status">
        <span>Running Status:</span>
        <span class="value">${version.running_status}</span>
      </div>
      <div class="serviceworker-fetch-handler-existence">
        <span>Fetch handler existence:</span>
        <span>${version.fetch_handler_existence}</span>
      </div>
      <div class="serviceworker-fetch-handler-type">
        <span>Fetch handler type:</span>
        <span>${version.fetch_handler_type}</span>
      </div>
      ${version.router_rules ? html`
        <div class="serviceworker-router-rules">
          <span>Static router rules:</span>
          <span>${version.router_rules}</span>
        </div>
      ` : ''}
      <div class="serviceworker-script_url">
        <span>Script:</span>
        <span>${version.script_url}</span>
      </div>
      <div class="serviceworker-vid">
        <span>Version ID:</span>
        <span class="value">${version.version_id}</span>
      </div>
      <div class="serviceworker-pid">
        <span>Renderer process ID:</span>
        <span class="value">${version.process_id}</span>
      </div>
      <div class="serviceworker-tid">
        <span>Renderer thread ID:</span>
        <span>${version.thread_id}</span>
      </div>
      <div class="serviceworker-rid">
        <span>DevTools agent route ID:</span>
        <span>${version.devtools_agent_route_id}</span>
      </div>
      ${version.clients.map(item => html`
        <div class="serviceworker-clients">
          <div>Client: </div>
          <div class="serviceworker-client">
            <div>ID: ${item.client_id}</div>
            <div>URL: ${item.url}</div>
          </div>
        </div>
      `)}
      <div>
        <div>Log:</div>
        <textarea class="serviceworker-log" rows="3" cols="120" readonly
            .value="${getLogsForversion(partitionId, version)}"></textarea>
      </div>
      <div class="worker-controls">
        ${version.running_status === 'RUNNING' ? html`
          <button data-command="stop"
              @click="${onButtonClick.bind(null, {
                partition_id: partitionId,
                version_id: version.version_id,
              })}">
            Stop
          </button>
          <button data-command="inspect"
              @click="${onButtonClick.bind(null, {
                process_host_id: version.process_host_id,
                devtools_agent_route_id: version.devtools_agent_route_id,
              })}">
            Inspect
          </button>
        ` : ''}
      </div>
    </div>`;
  // clang-format on
}

function getRegistrationHtml(registration: Registration, partitionId: number) {
  // clang-format off
  return html`
    <div class="serviceworker-registration"
        data-registration-id="${registration.registration_id}">
      <div class="serviceworker-scope">
        <span>Scope:</span>
        <span class="value">${registration.scope}</span>
      </div>
      <!-- Storage Partitioning -->
      ${registration.third_party_storage_partitioning_enabled ? html`
        <div class="serviceworker-storage-key-wrapper">
          Storage key:
          <div class="serviceworker-storage-key">
            <div class="serviceworker-origin">
              <span>Origin:</span>
              <span class="value">${registration.origin}</span>
            </div>
            <div class="serviceworker-top-level-site">
              <span>Top level site:</span>
              <span class="value">${registration.top_level_site}</span>
            </div>
            <div class="serviceworker-ancestor-chain-bit">
              <span>Ancestor chain bit:</span>
              <span class="value">${registration.ancestor_chain_bit}</span>
            </div>
            ${registration.nonce !== '<null>' ? html `
              <div class="serviceworker-nonce">
                <span>Nonce:</span>
                <span>${registration.nonce}</span>
              </div>
            ` : ''}
          </div>
        </div>
      ` : ''}
      <!-- Storage Partitioning ends -->
      <div class="serviceworker-rid">
        <span>Registration ID:</span>
        <span>${registration.registration_id}</span>
        <span ?hidden="${!registration.unregistered}">(unregistered)</span>
      </div>
      <div class="serviceworker-navigation-preload-enabled">
        <span>Navigation preload enabled:</span>
        <span>${registration.navigation_preload_enabled}</span>
      </div>
      <div class="serviceworker-navigation-preload-header-length">
        <span>Navigation preload header length:</span>
        <span>${registration.navigation_preload_header_length}</span>
      </div>

      ${registration.active ? html`
        <div>
          Active worker:
          ${getVersionHtml(registration.active, partitionId)}
        </div>
      ` : ''}

      ${registration.waiting ? html`
        <div>
          Waiting worker:
          ${getVersionHtml(registration.waiting, partitionId)}
        </div>
      ` : ''}

      ${!registration.unregistered ? html`
        <div class="registration-controls">
          <button data-command="unregister"
              @click="${onButtonClick.bind(null, {
                partition_id: partitionId,
                scope: registration.scope,
                storage_key: registration.storage_key,
              })}">
            Unregister
          </button>
          ${registration.active?.running_status !== 'RUNNING' ? html`
            <button data-command="start"
                @click="${onButtonClick.bind(null, {
                  partition_id: partitionId,
                  scope: registration.scope,
                  storage_key: registration.storage_key,
                })}">
              Start
            </button>
          ` : ''}
        </div>
      ` : ''}
    </div>`;
  // clang-format on
}

function getServiceWorkerListHtml(data: PartitionDataProcessed) {
  if (data.storedRegistrations.length + data.unregisteredRegistrations.length +
          data.unregisteredVersions.length ===
      0) {
    return html``;
  }

  // clang-format off
  return html`
    <div class="serviceworker-summary">
      <span>
        ${data.partitionPath !== '' ? html`
          <span>Registrations in: </span>
          <span>${data.partitionPath}</span>
        ` : html`
          <span>Registrations: Incognito </span>
        `}
      </span>
      <span>(${data.storedRegistrations.length})</span>
    </div>
    ${data.storedRegistrations.map(item => html`
      <div class="serviceworker-item">
        ${getRegistrationHtml(item, data.partitionId)}
      </div>
    `)}
    ${data.unregisteredRegistrations.map(item => html`
      <div class="serviceworker-item">
        ${getRegistrationHtml(item, data.partitionId)}
      </div>
    `)}
    ${data.unregisteredVersions.map(item => html`
      <div class="serviceworker-item">
        Unregistered worker:
        ${getVersionHtml(item, data.partitionId)}
      </div>
    `)}`;
  // clang-format on
}

function getServiceWorkerOptionsHtml(options: Options) {
  // clang-format off
  return html`
    <div class="checkbox">
      <label>
        <input type="checkbox" ?checked="${options.debug_on_start}"
            @change="${onDebugOnStartChange}">
          <span>
            Open DevTools window and pause JavaScript execution on Service Worker startup for debugging.
          </span>
      </label>
    </div>
  </div>`;
  // clang-format on
}

function onDebugOnStartChange(e: Event) {
  const input = e.target as HTMLInputElement;
  chrome.send('SetOption', ['debug_on_start', input.checked]);
}

function onOptions(options: Options) {
  render(
      getServiceWorkerOptionsHtml(options),
      getRequiredElement('serviceworker-options'));
}

async function onButtonClick(cmdArgs: Record<string, any>, e: Event) {
  const command = (e.target as HTMLElement).dataset['command'];
  assert(command);
  assert(COMMANDS.includes(command));
  await sendWithPromise(command, cmdArgs);
  update();
}

function getLogsForversion(partitionId: number, version: Version): string {
  const logMessages = allLogMessages.get(partitionId) || null;
  if (logMessages === null) {
    return '';
  }

  return logMessages.get(version.version_id) || '';
}

function addLogForversion(
    partitionId: number, versionId: string, message: string) {
  let logMessages = allLogMessages.get(partitionId) || null;
  if (logMessages === null) {
    logMessages = new Map();
    allLogMessages.set(partitionId, logMessages);
  }

  const previous = logMessages.get(versionId) || '';
  logMessages.set(versionId, previous + message);
}


// Get the unregistered workers.
// |unregisteredRegistrations| will be filled with the registrations which
// are in |liveRegistrations| but not in |storedRegistrations|.
// |unregisteredVersions| will be filled with the versions which
// are in |liveVersions| but not in |storedRegistrations| nor in
// |liveRegistrations|.
function getUnregisteredWorkers(
    storedRegistrations: Registration[], liveRegistrations: Registration[],
    liveVersions: Version[], unregisteredRegistrations: Registration[],
    unregisteredVersions: Version[]) {
  const registrationIdSet = new Set<string>();
  const versionIdSet = new Set<string>();
  storedRegistrations.forEach(function(registration) {
    registrationIdSet.add(registration.registration_id);
  });
  [storedRegistrations, liveRegistrations].forEach(function(registrations) {
    registrations.forEach(function(registration) {
      [registration.active, registration.waiting].forEach(function(version) {
        if (version) {
          versionIdSet.add(version.version_id);
        }
      });
    });
  });
  liveRegistrations.forEach(function(registration) {
    if (!registrationIdSet.has(registration.registration_id)) {
      registration.unregistered = true;
      unregisteredRegistrations.push(registration);
    }
  });
  liveVersions.forEach(function(version: Version) {
    if (!versionIdSet.has(version.version_id)) {
      unregisteredVersions.push(version);
    }
  });
}

// Fired once per partition from the backend.
function onPartitionData(
    registrations: PartitionData, partitionId: number, partitionPath: string) {
  // Update data model for `partitionId`.
  partitionsData.set(partitionId, {registrations, partitionPath});

  // Trigger re-rendering of corresponding DOM subtree.
  renderPartitionData(partitionId);
}

function renderPartitionData(partitionId: number) {
  const entry = partitionsData.get(partitionId) || null;
  assert(entry);

  const unregisteredRegistrations: Registration[] = [];
  const unregisteredVersions: Version[] = [];
  getUnregisteredWorkers(
      entry.registrations.storedRegistrations,
      entry.registrations.liveRegistrations, entry.registrations.liveVersions,
      unregisteredRegistrations, unregisteredVersions);

  const container = getRequiredElement('serviceworker-list');

  // Existing instantiated templates are keyed by `partitionId`. This allows the
  // UI of a given partition to be updated in-place rather than refreshing the
  // whole page.
  let partitionDiv = container.querySelector<HTMLElement>(
      `[data-partition-id='${partitionId}']`);

  if (partitionDiv === null) {
    // First time rendering for `partitionId`, create a DOM node for it.
    partitionDiv = document.createElement('div');
    partitionDiv.dataset['partitionId'] = String(partitionId);
    container.appendChild(partitionDiv);
  }

  assert(partitionDiv);
  render(
      getServiceWorkerListHtml({
        storedRegistrations: entry.registrations.storedRegistrations,
        unregisteredRegistrations,
        unregisteredVersions,
        partitionId,
        partitionPath: entry.partitionPath,
      }),
      partitionDiv);
}

function onErrorReported(
    partitionId: number, versionId: string, errorInfo: any) {
  // Update data model.
  addLogForversion(
      partitionId, versionId, 'Error: ' + JSON.stringify(errorInfo) + '\n');

  // Trigger re-rendering of corresponding DOM subtree.
  renderPartitionData(partitionId);
}

function onConsoleMessageReported(
    partitionId: number, versionId: string, message: any) {
  // Update data model.
  addLogForversion(
      partitionId, versionId, 'Console: ' + JSON.stringify(message) + '\n');

  // Trigger re-rendering of corresponding DOM subtree.
  renderPartitionData(partitionId);
}


const COMMANDS: string[] = ['stop', 'inspect', 'unregister', 'start'];
const allLogMessages = new Map<number, Map<string, string>>();
const partitionsData: Map<number, PartitionsDataEntry> = new Map();

function initialize() {
  addWebUiListener('partition-data', onPartitionData);
  addWebUiListener('running-state-changed', update);
  addWebUiListener('error-reported', onErrorReported);
  addWebUiListener('console-message-reported', onConsoleMessageReported);
  addWebUiListener('version-state-changed', update);
  addWebUiListener('version-router-rules-changed', update);
  addWebUiListener('registration-completed', update);
  addWebUiListener('registration-deleted', update);
  update();
}

function update() {
  sendWithPromise('GetOptions').then(onOptions);
  chrome.send('getAllRegistrations');
}

document.addEventListener('DOMContentLoaded', initialize);
