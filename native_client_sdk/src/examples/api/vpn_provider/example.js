// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VPN Configuration identification.
var configName = 'Mock configuration';
var configId;

// Example configuration.
var vpnParams = {
  'address': '127.0.0.1/32',
  'mtu': '1000',
  'exclusionList': ['127.0.0.1/32'],
  'inclusionList': ['0.0.0.0/0'],
  'dnsServers': ['8.8.8.8'],
  'reconnect': 'true'
};

// Simple log to HTML
function wlog(message) {
  var logEl = document.getElementById('log');
  logEl.innerHTML += message + '<br />'; // Append to log.
  logEl.scrollTop = logEl.scrollHeight;  // Scroll log to bottom.
}

// Create a VPN configuration using the createConfig method.
// A VPN configuration is a persistent entry shown to the user in a native
// Chrome OS UI. The user can select a VPN configuration from a list and
// connect to it or disconnect from it.
function create() {
  chrome.vpnProvider.createConfig(configName, function(id) {
    configId = id;
    wlog('JS: Created configuration with name=\'' + configName + '\'' +
         ' and id=\'' + configId + '\'');
  });
}

// Bind connection to NaCl.
function bind() {
  common.naclModule.postMessage({cmd: 'bind', name: configName, id: configId});
}

function onSetParameters() {
  chrome.vpnProvider.setParameters(vpnParams, function() {
    wlog('JS: setParameters set!');

    // Bind connection to NaCl.
    bind();
  });
}

function onBindSuccess() {
  // Notify the connection state as 'connected'.
  chrome.vpnProvider.notifyConnectionStateChanged('connected', function() {
    wlog('JS: notifyConnectionStateChanged connected!');
  });
}

// VpnProviders handlers.
function onPlatformMessageListener(id, message, error) {
  wlog('JS: onPlatformMessage: id=\'' + id + '\' message=\'' + message +
       '\' error=\'' + error + '\'');

  if (message == 'connected') {
    wlog('JS: onPlatformMessage  connected!');

    // Notify NaCl module to connect to the VPN tunnel.
    common.naclModule.postMessage({cmd: 'connected'});

  } else if (message == 'disconnected') {
    wlog('JS: onPlatformMessage  disconnected!');

    // Notify NaCl module to disconnect from the VPN tunnel.
    common.naclModule.postMessage({cmd: 'disconnected'});
  }
}

// This function is called by common.js when a message is received from the
// NaCl module.
function handleMessage(message) {
  if (typeof message.data === 'string') {
    wlog(message.data);
  } else if (message.data['cmd'] == 'setParameters') {
    onSetParameters();
  } else if (message.data['cmd'] == 'bindSuccess') {
    onBindSuccess();
  }
}

// setupHandlers VpnProviders handlers.
function setupHandlers() {
  // Add listeners to the events onPlatformMessage, onPacketReceived and
  // onConfigRemoved.
  chrome.vpnProvider.onPlatformMessage.addListener(onPlatformMessageListener);

  chrome.vpnProvider.onPacketReceived.addListener(function(data) {
    wlog('JS: onPacketReceived');
    console.log('Unexpected event:vpnProvider.onPacketReceived ' +
      'called from JavaScript.');
  });

  chrome.vpnProvider.onConfigRemoved.addListener(function(id) {
    wlog('JS: onConfigRemoved: id=\'' + id + '\'');
  });

  chrome.vpnProvider.onConfigCreated.addListener(function(id, name, data) {
    wlog('JS: onConfigCreated: id=\'' + id + '\' name=\'' + name + '\'' +
         'data=' + JSON.stringify(data));
  });

  chrome.vpnProvider.onUIEvent.addListener(function(event, id) {
    wlog('JS: onUIEvent: event=\'' + event + '\' id=\'' + id + '\'');
  });
}

// This function is called by common.js when the NaCl module is
// loaded.
function moduleDidLoad() {
  // Once we load, hide the plugin. In this example, we don't display anything
  // in the plugin, so it is fine to hide it.
  common.hideModule();

  if (chrome.vpnProvider === undefined) {
    wlog('JS: moduleDidLoad: chrome.vpnProvider undefined.');
    console.log('JS: moduleDidLoad: chrome.vpnProvider undefined.');
    return;
  }

  // Setup VpnProvider handlers.
  setupHandlers();

  // All done, create the connection entry in the VPN UI.
  create();
}
