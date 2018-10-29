// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('serviceworker', function() {
  'use strict';

  function initialize() {
    update();
  }

  function update() {
    chrome.send('GetOptions');
    chrome.send('getAllRegistrations');
  }

  function onOptions(options) {
    var template;
    var container = $('serviceworker-options');
    if (container.childNodes) {
      template = container.childNodes[0];
    }
    if (!template) {
      template = jstGetTemplate('serviceworker-options-template');
      container.appendChild(template);
    }
    jstProcess(new JsEvalContext(options), template);
    var inputs = container.querySelectorAll('input[type=\'checkbox\']');
    for (var i = 0; i < inputs.length; ++i) {
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
  var COMMANDS = ['stop', 'inspect', 'unregister', 'start'];
  function commandHandler(command) {
    return function(event) {
      var link = event.target;
      progressNodeFor(link).style.display = 'inline';
      sendCommand(command, link.cmdArgs, (function(status) {
                                           progressNodeFor(link).style.display =
                                               'none';
                                         }).bind(null, link));
      return false;
    };
  };

  var commandCallbacks = [];
  function sendCommand(command, args, callback) {
    var callbackId = 0;
    while (callbackId in commandCallbacks) {
      callbackId++;
    }
    commandCallbacks[callbackId] = callback;
    chrome.send(command, [callbackId, args]);
  }

  // Fired from the backend after the command call has completed.
  function onOperationComplete(status, callbackId) {
    var callback = commandCallbacks[callbackId];
    delete commandCallbacks[callbackId];
    if (callback) {
      callback(status);
    }
    update();
  }

  var allLogMessages = {};
  // Set log for a worker version.
  function fillLogForVersion(container, partition_id, version) {
    if (!version) {
      return;
    }
    if (!(partition_id in allLogMessages)) {
      allLogMessages[partition_id] = {};
    }
    var logMessages = allLogMessages[partition_id];
    if (version.version_id in logMessages) {
      version.log = logMessages[version.version_id];
    } else {
      version.log = '';
    }
    var logAreas = container.querySelectorAll('textarea.serviceworker-log');
    for (var i = 0; i < logAreas.length; ++i) {
      var logArea = logAreas[i];
      if (logArea.partition_id == partition_id &&
          logArea.version_id == version.version_id) {
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
    var registration_id_set = {};
    var version_id_set = {};
    stored_registrations.forEach(function(registration) {
      registration_id_set[registration.registration_id] = true;
    });
    [stored_registrations, live_registrations].forEach(function(registrations) {
      registrations.forEach(function(registration) {
        [registration.active, registration.waiting].forEach(function(version) {
          if (version) {
            version_id_set[version.version_id] = true;
          }
        });
      });
    });
    live_registrations.forEach(function(registration) {
      if (!registration_id_set[registration.registration_id]) {
        registration.unregistered = true;
        unregistered_registrations.push(registration);
      }
    });
    live_versions.forEach(function(version) {
      if (!version_id_set[version.version_id]) {
        unregistered_versions.push(version);
      }
    });
  }

  // Fired once per partition from the backend.
  function onPartitionData(
      live_registrations, live_versions, stored_registrations, partition_id,
      partition_path) {
    var unregistered_registrations = [];
    var unregistered_versions = [];
    getUnregisteredWorkers(
        stored_registrations, live_registrations, live_versions,
        unregistered_registrations, unregistered_versions);
    var template;
    var container = $('serviceworker-list');
    // Existing templates are keyed by partition_id. This allows
    // the UI to be updated in-place rather than refreshing the
    // whole page.
    for (var i = 0; i < container.childNodes.length; ++i) {
      if (container.childNodes[i].partition_id == partition_id) {
        template = container.childNodes[i];
      }
    }
    // This is probably the first time we're loading.
    if (!template) {
      template = jstGetTemplate('serviceworker-list-template');
      container.appendChild(template);
    }
    var fillLogFunc = fillLogForVersion.bind(this, container, partition_id);
    stored_registrations.forEach(function(registration) {
      [registration.active, registration.waiting].forEach(fillLogFunc);
    });
    unregistered_registrations.forEach(function(registration) {
      [registration.active, registration.waiting].forEach(fillLogFunc);
    });
    unregistered_versions.forEach(fillLogFunc);
    jstProcess(
        new JsEvalContext({
          stored_registrations: stored_registrations,
          unregistered_registrations: unregistered_registrations,
          unregistered_versions: unregistered_versions,
          partition_id: partition_id,
          partition_path: partition_path
        }),
        template);
    for (var i = 0; i < COMMANDS.length; ++i) {
      var handler = commandHandler(COMMANDS[i]);
      var links = container.querySelectorAll('button.' + COMMANDS[i]);
      for (var j = 0; j < links.length; ++j) {
        if (!links[j].hasClickEvent) {
          links[j].addEventListener('click', handler, false);
          links[j].hasClickEvent = true;
        }
      }
    }
  }

  function onRunningStateChanged(partition_id, version_id) {
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
    var logMessages = allLogMessages[partition_id];
    if (version_id in logMessages) {
      logMessages[version_id] += message;
    } else {
      logMessages[version_id] = message;
    }

    var logAreas = document.querySelectorAll('textarea.serviceworker-log');
    for (var i = 0; i < logAreas.length; ++i) {
      var logArea = logAreas[i];
      if (logArea.partition_id == partition_id &&
          logArea.version_id == version_id) {
        logArea.value += message;
      }
    }
  }

  return {
    initialize: initialize,
    onOptions: onOptions,
    onOperationComplete: onOperationComplete,
    onPartitionData: onPartitionData,
    onRunningStateChanged: onRunningStateChanged,
    onErrorReported: onErrorReported,
    onConsoleMessageReported: onConsoleMessageReported,
    onVersionStateChanged: onVersionStateChanged,
    onRegistrationCompleted: onRegistrationCompleted,
    onRegistrationDeleted: onRegistrationDeleted,
  };
});

document.addEventListener('DOMContentLoaded', serviceworker.initialize);
