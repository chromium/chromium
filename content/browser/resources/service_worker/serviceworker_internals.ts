// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {assert} from 'chrome://resources/js/assert.js';
import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

function initialize() {
  addWebUiListener('partition-data', onPartitionData);
  addWebUiListener('running-state-changed', onRunningStateChanged);
  addWebUiListener('error-reported', onErrorReported);
  addWebUiListener('console-message-reported', onConsoleMessageReported);
  addWebUiListener('version-state-changed', onVersionStateChanged);
  addWebUiListener('version-router-rules-changed', onVersionRouterRulesChanged);
  addWebUiListener('registration-completed', onRegistrationCompleted);
  addWebUiListener('registration-deleted', onRegistrationDeleted);
  update();
}

function update() {
  sendWithPromise('GetOptions').then(onOptions);
  chrome.send('getAllRegistrations');
}

interface Options {
  debug_on_start: boolean;
}

interface Registration {
  active: Version;
  registration_id: string;
  unregistered: boolean;
  waiting: Version;
}

interface Version {
  version_id: string;
  log: string;
}

interface PartitionData {
  liveRegistrations: Registration[];
  storedRegistrations: Registration[];
  liveVersions: Version[];
}

interface CmdArgs {}

interface WithPartitionId {
  partition_id: number;
}

interface WithVersionId {
  version_id: string;
}

interface WithCmdArgs {
  cmdArgs: CmdArgs;
}

function onOptions(options: Options) {
  let template: HTMLElement|undefined;
  const container = getRequiredElement('serviceworker-options');
  template = container.children[0] as HTMLElement|undefined;

  if (!template) {
    template = jstGetTemplate('serviceworker-options-template');
    container.appendChild(template);
  }
  assert(template);
  jstProcess(new JsEvalContext(options), template);
  const inputs =
      container.querySelectorAll<HTMLInputElement>('input[type=\'checkbox\']');
  for (const input of inputs) {
    input.onclick = _event => {
      chrome.send('SetOption', [input.className, input.checked]);
    };
  }
}

function progressNodeFor(link: HTMLElement): HTMLElement {
  const node = link.parentNode!.querySelector<HTMLElement>('.operation-status');
  assert(node);
  return node;
}

// All commands are completed with 'onOperationComplete'.
const COMMANDS: string[] = ['stop', 'inspect', 'unregister', 'start'];

function commandHandler(command: string) {
  return function(event: Event) {
    const link = event.target as HTMLElement & WithCmdArgs;
    progressNodeFor(link).style.display = 'inline';
    sendWithPromise(command, link.cmdArgs).then(() => {
      progressNodeFor(link).style.display = 'none';
      update();
    });
    return false;
  };
}

const allLogMessages = new Map<number, Map<string, string>>();

// Set log for a worker version.
function fillLogForVersion(
    container: HTMLElement, partitionId: number, version: Version) {
  if (!version) {
    return;
  }

  let logMessages = allLogMessages.get(partitionId) || null;
  if (logMessages === null) {
    logMessages = new Map();
    allLogMessages.set(partitionId, logMessages);
  }

  version.log = logMessages.get(version.version_id) || '';

  const logAreas =
      container
          .querySelectorAll<HTMLTextAreaElement&WithPartitionId&WithVersionId>(
              'textarea.serviceworker-log');
  for (const logArea of logAreas) {
    if (logArea.partition_id === partitionId &&
        logArea.version_id === version.version_id) {
      logArea.value = version.log;
    }
  }
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
  const unregisteredRegistrations: Registration[] = [];
  const unregisteredVersions: Version[] = [];
  const storedRegistrations = registrations.storedRegistrations;
  getUnregisteredWorkers(
      storedRegistrations, registrations.liveRegistrations,
      registrations.liveVersions, unregisteredRegistrations,
      unregisteredVersions);
  let template: HTMLElement|undefined;
  const container = getRequiredElement('serviceworker-list');
  // Existing templates are keyed by partition_id. This allows
  // the UI to be updated in-place rather than refreshing the
  // whole page.
  for (let i = 0; i < container.childNodes.length; ++i) {
    if ((container.children[i] as HTMLElement & WithPartitionId)
            .partition_id === partitionId) {
      template = container.children[i] as HTMLElement;
    }
  }
  // This is probably the first time we're loading.
  if (!template) {
    template = jstGetTemplate('serviceworker-list-template');
    container.appendChild(template);
  }
  const fillLogFunc = fillLogForVersion.bind(null, container, partitionId);
  storedRegistrations.forEach(function(registration) {
    [registration.active, registration.waiting].forEach(fillLogFunc);
  });
  unregisteredRegistrations.forEach(function(registration) {
    [registration.active, registration.waiting].forEach(fillLogFunc);
  });
  unregisteredVersions.forEach(fillLogFunc);
  jstProcess(
      new JsEvalContext({
        stored_registrations: storedRegistrations,
        unregistered_registrations: unregisteredRegistrations,
        unregistered_versions: unregisteredVersions,
        partition_id: partitionId,
        partition_path: partitionPath,
      }),
      template);
  for (const command of COMMANDS) {
    const handler = commandHandler(command);
    const links = container.querySelectorAll<HTMLElement>('button.' + command);
    for (const link of links) {
      link.onclick = handler;
    }
  }
}

function onRunningStateChanged() {
  update();
}

function onErrorReported(
    partitionId: number, versionId: string, errorInfo: any) {
  outputLogMessage(
      partitionId, versionId, 'Error: ' + JSON.stringify(errorInfo) + '\n');
}

function onConsoleMessageReported(
    partitionId: number, versionId: string, message: any) {
  outputLogMessage(
      partitionId, versionId, 'Console: ' + JSON.stringify(message) + '\n');
}

function onVersionStateChanged() {
  update();
}

function onVersionRouterRulesChanged() {
  update();
}

function onRegistrationCompleted() {
  update();
}

function onRegistrationDeleted() {
  update();
}

function outputLogMessage(
    partitionId: number, versionId: string, message: string) {
  let logMessages = allLogMessages.get(partitionId) || null;
  if (logMessages === null) {
    logMessages = new Map();
    allLogMessages.set(partitionId, logMessages);
  }

  logMessages.set(versionId, (logMessages.get(versionId) || '') + message);

  const logAreas =
      document
          .querySelectorAll<HTMLTextAreaElement&WithPartitionId&WithVersionId>(
              'textarea.serviceworker-log');
  for (const logArea of logAreas) {
    if (logArea.partition_id === partitionId &&
        logArea.version_id === versionId) {
      logArea.value += message;
    }
  }
}

document.addEventListener('DOMContentLoaded', initialize);
