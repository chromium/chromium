// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that each agent could be enabled/disabled separately.\n`);

  function printResult(agentName, action, errorString) {
    if (action === 'enable')
      TestRunner.addResult('');
    if (errorString)
      TestRunner.addResult(agentName + '.' + action + ' finished with error ' + errorString);
    else
      TestRunner.addResult(agentName + '.' + action + ' finished successfully');
  }

  var targets = SDK.targetManager.targets();
  for (var target of targets) {
    var agentNames = Object.keys(target._agents)
                         .filter(function(agentName) {
                           var agent = target._agents[agentName];
                           return agent['enable'] && agent['disable']
                               && agentName !== 'ServiceWorker'
                               && agentName !== 'Security'
                               && agentName !== 'Inspector'
                               && agentName !== 'HeadlessExperimental'
                               && agentName !== 'Fetch'
                               && agentName !== 'Cast'
                               && agentName !== 'BackgroundService';
                         })
                         .sort();

    async function disableAgent(agentName) {
      var agent = target._agents[agentName];
      var response = await agent.invoke_disable({});
      printResult(agentName, 'disable', response[Protocol.Error]);
    }

    async function enableAgent(agentName) {
      var agent = target._agents[agentName];
      var response = await agent.invoke_enable({});
      printResult(agentName, 'enable', response[Protocol.Error]);
    }

    for (agentName of agentNames)
      await disableAgent(agentName);

    for (agentName of agentNames) {
      await enableAgent(agentName);
      await disableAgent(agentName);
    }
  }

  TestRunner.completeTest();
})();
