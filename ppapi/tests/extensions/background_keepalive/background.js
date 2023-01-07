// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var modulesCreated = 0;

// Indicate load success.
function moduleDidLoad(moduleNumber) {
  chrome.test.sendMessage('created_module:' + moduleNumber.toString(),
                          handleChromeTestMessage);
}

var handleChromeTestMessage = function (message) {
  if (message == 'create_module') {
    createNaClEmbed(true);
  } else if (message == 'create_module_without_hack') {
    createNaClEmbed(false);
  } else if (message == 'destroy_module') {
    destroyNaClEmbed();
  }
}

function handleNaclMessage(messageEvent) {
  console.log('handleNaclMessage: ' + messageEvent.data);
}

function createNaClEmbed(touchEmbedHack) {
  modulesCreated++;

  var embed = document.createElement('embed');
  embed.src = 'ppapi_tests_extensions_background_keepalive.nmf';
  embed.type = 'application/x-nacl';

  var listener = document.createElement('div');
  listener.addEventListener('load', moduleDidLoad.bind(null, modulesCreated),
                            true);
  listener.addEventListener('message', handleNaclMessage, true);
  listener.appendChild(embed);

  document.body.appendChild(listener);

  if (touchEmbedHack)
    console.assert(embed.lastError == 0);
}

function destroyNaClEmbed() {
  moduleDivs = document.querySelectorAll('div');
  console.assert(moduleDivs.length > 0);
  document.body.removeChild(moduleDivs[0]);

  chrome.test.sendMessage('destroyed_module', handleChromeTestMessage);
}

chrome.test.sendMessage('ready', handleChromeTestMessage);
