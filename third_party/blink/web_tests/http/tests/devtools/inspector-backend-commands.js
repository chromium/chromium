// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as ProtocolClient from 'devtools/core/protocol_client/protocol_client.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests correctness of promisified protocol commands.\n`);

  ProtocolClient.InspectorBackend.test.suppressRequestErrors = false;
  function dumpArgument(name, value) {
    TestRunner.addResult(name + ' result: ' + (typeof value === 'string' ? value : JSON.stringify(value)));
  }

  function processResult(name, promise) {
    return promise.then(dumpArgument.bind(null, name + ': then'), dumpArgument.bind(null, name + ': catch'));
  }

  function sendMessageToBackendLoopback(message) {
    var messageObject = JSON.parse(message);
    messageObject.result = messageObject.params;
    messageObject.error = messageObject.params && messageObject.params.error;
    var response = JSON.stringify(messageObject);
    setTimeout(InspectorFrontendAPI.dispatchMessage.bind(InspectorFrontendAPI, response), 0);
  }

  var inspectorJson = {
    'domains': [{
      'domain': 'Profiler',
      'commands': [
        {'name': 'commandArgs0'}, {'name': 'commandArgs1Rets0', 'parameters': [{'name': 'arg1', 'type': 'number'}]}, {
          'name': 'commandArgs1Rets1',
          'parameters': [{'name': 'arg1', 'type': 'object'}],
          'returns': [{'name': 'arg1', 'type': 'object'}]
        },
        {
          'name': 'commandArgs3Rets3',
          'parameters': [
            {'name': 'arg1', 'type': 'object'}, {'name': 'arg2', 'type': 'number', 'optional': true},
            {'name': 'arg3', 'type': 'string', 'optional': true}
          ],
          'returns': [
            {'name': 'arg1', 'type': 'object'}, {'name': 'arg2', 'type': 'number'},
            {'name': 'arg3', 'type': 'string'}
          ]
        },
        {'name': 'commandError', 'parameters': [{'name': 'error', 'type': 'object'}]}
      ]
    }]
  };
  // The protocol definition above is not used, but is left as a reference for commands below.
  ProtocolClient.InspectorBackend.inspectorBackend.registerCommand('Profiler.commandArgs0', [], []);
  ProtocolClient.InspectorBackend.inspectorBackend.registerCommand(
      'Profiler.commandArgs1Rets0', [{'name': 'arg1', 'type': 'number', 'optional': false}], []);
  ProtocolClient.InspectorBackend.inspectorBackend.registerCommand(
      'Profiler.commandArgs1Rets1', [{'name': 'arg1', 'type': 'object', 'optional': false}], ['arg1']);
  ProtocolClient.InspectorBackend.inspectorBackend.registerCommand(
      'Profiler.commandArgs3Rets3',
      [
        {'name': 'arg1', 'type': 'object', 'optional': false}, {'name': 'arg2', 'type': 'number', 'optional': true},
        {'name': 'arg3', 'type': 'string', 'optional': true}
      ],
      ['arg1', 'arg2', 'arg3']);
  ProtocolClient.InspectorBackend.inspectorBackend.registerCommand(
      'Profiler.commandError', [{'name': 'error', 'type': 'object', 'optional': false}], []);

  var sendMessageToBackendOriginal = InspectorFrontendHost.sendMessageToBackend;
  InspectorFrontendHost.sendMessageToBackend = sendMessageToBackendLoopback;

  var agent = SDK.TargetManager.TargetManager.instance().primaryPageTarget().profilerAgent();
  await processResult(
      'commandError',
      agent.commandError({'message': 'this is the error message'}));  // Error: error in the protocol response
  await processResult('commandArgs0', agent.commandArgs0());
  await processResult('commandArgs0', agent.commandArgs0(1));  // Error: extra arg
  await processResult('commandArgs1Rets0', agent.commandArgs1Rets0(123));
  await processResult('commandArgs1Rets0', agent.commandArgs1Rets0(123, 456));  // Error: extra arg
  await processResult('commandArgs1Rets0', agent.commandArgs1Rets0('abc'));     // Error: wrong type
  await processResult('commandArgs1Rets0', agent.commandArgs1Rets0());          // Error: missing arg
  await processResult('commandArgs1Rets1', agent.commandArgs1Rets1({'value1': 234}));
  await processResult('commandArgs1Rets1', agent.commandArgs1Rets1({'value1': 234}));
  await processResult('commandArgs3Rets3', agent.invoke_commandArgs3Rets3({arg1: {}, arg2: 345, arg3: 'alph'}));
  await processResult('commandArgs3Rets3', agent.commandArgs3Rets3({}, 345));
  await processResult('commandArgs3Rets3', agent.commandArgs3Rets3({}, undefined, 'alph'));
  await processResult(
      'commandArgs3Rets3', agent.invoke_commandArgs3Rets3({arg1: {}, arg2: 'alph'}));  // Error: wrong type
  await processResult('commandArgs3Rets3', agent.commandArgs3Rets3({}));
  await processResult('commandArgs3Rets3', agent.commandArgs3Rets3());                      // Error: missing arg
  await processResult('commandArgs3Rets3', agent.commandArgs3Rets3({}, 'alph', 345));       // Error: wrong types
  await processResult('commandArgs3Rets3', agent.commandArgs3Rets3({}, 'alph', 345, 678));  // Error: extra arg

  InspectorFrontendHost.sendMessageToBackend = sendMessageToBackendOriginal;
  TestRunner.completeTest();
})();
