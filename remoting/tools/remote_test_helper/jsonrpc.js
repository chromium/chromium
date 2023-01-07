// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var jsonRpc = {};
jsonRpc.responseObject = null;

jsonRpc.setLastEvent = function(action, value, modifiers) {
  var request = jsonRpc.generateJsonRpcRequest(
    'SetLastEvent', [action, value, modifiers]);
  return jsonRpc.sendRpcRequest(request);
}

jsonRpc.getLastEvent = function() {
  var request = jsonRpc.generateJsonRpcRequest('GetLastEvent', []);
  return jsonRpc.sendRpcRequest(request);
}

jsonRpc.clearLastEvent = function() {
  var request = jsonRpc.generateJsonRpcRequest('ClearLastEvent', []);
  return jsonRpc.sendRpcRequest(request);
}

/**
 * Generate the JSON request.
 * @param {string} methodname The name of the remote method.
 * @param {list} params The method parameters to pass.
 * @param {number=} opt_ident The request id.
 * @return The JSON-RPC request object
 */
jsonRpc.generateJsonRpcRequest = function(methodname, params, opt_ident) {
  ident = opt_ident == undefined ? 0 : opt_ident;
  var request = {
    "jsonrpc": "2.0",
    "method": methodname,
    "params": params,
    "id": ident
    };
  return request;
}

/**
 * Method to POST the request to the RPC server.
 * @param {object} json_request The JSON request object.
 */
jsonRpc.sendRpcRequest = function(json_request) {
  jsonRpc.responseObject = null;
  var xhr = new XMLHttpRequest();
  xhr.open('POST', '/RPC2', true);
  xhr.onreadystatechange = function () {
    if (xhr.readyState == 4 && xhr.status == 200) {
      try {
        var response = xhr.responseText;
        jsonRpc.responseObject = JSON.parse(response).response;
      } catch (err) {
        console.error('Could not parse server response.');
        return;
      }
    }
  }
  string_request = JSON.stringify(json_request);
  xhr.send(string_request);
}
