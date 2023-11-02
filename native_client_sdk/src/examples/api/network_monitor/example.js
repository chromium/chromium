// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called by the common.js module.
function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();
}

// Called by the common.js module.
function handleMessage(message) {
  if (typeof message.data === "string") {
    // We got an error from the NaCl module.
    common.logMessage('Error: ' + message.data);
    return;
  }

  // We expect that the message looks like this:
  // [{
  //    name: "...", displayName: "...", state: "...", type: "...", MTU: 1234,
  //    ipAddresses: ["...", "..."]},
  //  {...},
  // ]
  //
  // Append a <tr> to the <tbody> for each interface in the array.
  // The order in the .html file is:
  // index, display name, name, type, state, ip addresses, MTU.
  var net_interfaces = message.data;

  var tbodyEl = document.querySelector('tbody');
  // First, clear the tbody.
  while (tbodyEl.firstChild) {
    tbodyEl.removeChild(tbodyEl.firstChild);
  }

  for (var i = 0; i < net_interfaces.length; ++i) {
    var net_interface = net_interfaces[i];
    var trEl = document.createElement('tr');
    trEl.appendChild(makeTd(i));
    trEl.appendChild(makeTd(net_interface.displayName));
    trEl.appendChild(makeTd(net_interface.name));
    trEl.appendChild(makeTd(net_interface.type));
    trEl.appendChild(makeTd(net_interface.state));
    // ipAddresses is an array of strings. Let's join them with a comma.
    trEl.appendChild(makeTd(net_interface.ipAddresses.join(', ')));
    trEl.appendChild(makeTd(net_interface.MTU));
    tbodyEl.appendChild(trEl);
  }
}

function makeTd(text) {
  var tdEl = document.createElement('td');
  tdEl.textContent = text;
  return tdEl;
}
