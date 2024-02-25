// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util.js';

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

  function onOptions(options) {
    let template;
    const container = $('serviceworker-options');
    if (container.childNodes) {
      template = container.childNodes[0];
    }
    if (!template) {
      template = jstGetTemplate('serviceworker-options-template');
      container.appendChild(template);
    }
    jstProcess(new JsEvalContext(options), template);
    const inputs = container.querySelectorAll('input[type=\'checkbox\']');
    for (let i = 0; i < inputs.length; ++i) {
      if (!inputs[i].hasClickEvent) {
        inputs[i].addEventListener(
            'click',
            (function(event) {
              chrome.send(
                  'SetOption', [event.target.className, event.target.checked]);
            }).bind(this),
            false);
        inputs[i].hasClickEvent = true;
      }
    }
  }

  function progressNodeFor(link) {
    return link.parentNode.querySelector('.operation-status');
  }

  // All commands are completed with 'onOperationComplete'.
  const COMMANDS = ['stop', 'inspect', 'unregister', 'start'];
  function commandHandler(command) {
    return function(event) {
      const link = event.target;
      progressNodeFor(link).style.display = 'inline';
      sendWithPromise(command, link.cmdArgs).then(() => {
        progressNodeFor(link).style.display = 'none';
        update();
      });
      return false;
    };
  }

  const allLogMessages = {};
  // Set log for a worker version.
  function fillLogForVersion(container, partition_id, version) {
    if (!version) {
      return;
    }
    if (!(partition_id in allLogMessages)) {
      allLogMessages[partition_id] = {};
    }
    const logMessages = allLogMessages[partition_id];
    if (version.version_id in logMessages) {
      version.log = logMessages[version.version_id];
    } else {
      version.log = '';
    }
    const logAreas = container.querySelectorAll('textarea.serviceworker-log');
    for (let i = 0; i < logAreas.length; ++i) {
      const logArea = logAreas[i];
      if (logArea.partition_id === partition_id &&
          logArea.version_id === version.version_id) {
        logArea.value = version.log;
      }
    }
  }

  // Get the unregistered workers.
  // |unregistered_registrations| will be filled with the registrations which
  // are in |live_registrations| but not in |stored_registrations|.
  // |unregistered_versions| will be filled with the versions which
  // are in |live_versions| but not in |stored_registrations| nor in
  // |live_registrations|.
  function getUnregisteredWorkers(
      stored_registrations, live_registrations, live_versions,
      unregistered_registrations, unregistered_versions) {
    const registrationIdSet = {};
    const versionIdSet = {};
    stored_registrations.forEach(function(registration) {
      registrationIdSet[registration.registration_id] = true;
    });
    [stored_registrations, live_registrations].forEach(function(registrations) {
      registrations.forEach(function(registration) {
        [registration.active, registration.waiting].forEach(function(version) {
          if (version) {
            versionIdSet[version.version_id] = true;
          }
        });
      });
    });
    live_registrations.forEach(function(registration) {
      if (!registrationIdSet[registration.registration_id]) {
        registration.unregistered = true;
        unregistered_registrations.push(registration);
      }
    });
    live_versions.forEach(function(version) {
      if (!versionIdSet[version.version_id]) {
        unregistered_versions.push(version);
      }
    });
  }

  // Fired once per partition from the backend.
  function onPartitionData(registrations, partition_id, partition_path) {
    const unregisteredRegistrations = [];
    const unregisteredVersions = [];
    const storedRegistrations = registrations.storedRegistrations;
    getUnregisteredWorkers(
        storedRegistrations, registrations.liveRegistrations,
        registrations.liveVersions, unregisteredRegistrations,
        unregisteredVersions);
    let template;
    const container = $('serviceworker-list');
    // Existing templates are keyed by partition_id. This allows
    // the UI to be updated in-place rather than refreshing the
    // whole page.
    for (let i = 0; i < container.childNodes.length; ++i) {
      if (container.childNodes[i].partition_id === partition_id) {
        template = container.childNodes[i];
      }
    }
    // This is probably the first time we're loading.
    if (!template) {
      template = jstGetTemplate('serviceworker-list-template');
      container.appendChild(template);
    }
    const fillLogFunc = fillLogForVersion.bind(this, container, partition_id);
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
          partition_id: partition_id,
          partition_path: partition_path,
        }),
        template);
    for (let i = 0; i < COMMANDS.length; ++i) {
      const handler = commandHandler(COMMANDS[i]);
      const links = container.querySelectorAll('button.' + COMMANDS[i]);
      for (let j = 0; j < links.length; ++j) {
        if (!links[j].hasClickEvent) {
          links[j].addEventListener('click', handler, false);
          links[j].hasClickEvent = true;
        }
      }
    }
  }

  function onRunningStateChanged() {
    update();
  }

  function onErrorReported(partition_id, version_id, error_info) {
    outputLogMessage(
        partition_id, version_id,
        'Error: ' + JSON.stringify(error_info) + '\n');
  }

  function onConsoleMessageReported(partition_id, version_id, message) {
    outputLogMessage(
        partition_id, version_id, 'Console: ' + JSON.stringify(message) + '\n');
  }

  function onVersionStateChanged(partition_id, version_id) {
    update();
  }

  function onVersionRouterRulesChanged() {
    update();
  }

  function onRegistrationCompleted(scope) {
    update();
  }

  function onRegistrationDeleted(scope) {
    update();
  }

  function outputLogMessage(partition_id, version_id, message) {
    if (!(partition_id in allLogMessages)) {
      allLogMessages[partition_id] = {};
    }
    const logMessages = allLogMessages[partition_id];
    if (version_id in logMessages) {
      logMessages[version_id] += message;
    } else {
      logMessages[version_id] = message;
    }

    const logAreas = document.querySelectorAll('textarea.serviceworker-log');
    for (let i = 0; i < logAreas.length; ++i) {
      const logArea = logAreas[i];
      if (logArea.partition_id === partition_id &&
          logArea.version_id === version_id) {
        logArea.value += message;
      }
    }
  }

  document.addEventListener('DOMContentLoaded', initialize);
