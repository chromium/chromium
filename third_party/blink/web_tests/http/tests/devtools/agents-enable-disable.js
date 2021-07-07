// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  'use strict';
  TestRunner.addResult(`Test that each agent could be enabled/disabled separately.\n`);

  function printResult(agentName, action, errorString) {
    if (action === 'enable')
      TestRunner.addResult('');
    if (errorString)
      TestRunner.addResult(agentName + '.' + action + ' finished with error ' + errorString);
    else
      TestRunner.addResult(agentName + '.' + action + ' finished successfully');
  }

  const targets = SDK.targetManager.targets();
  for (const target of targets) {
    const agentNames =
        Array.from(target.agents.keys())
            .filter(function(agentName) {
              const agent = target.agents.get(agentName);
              return agent['enable'] && agent['disable'] &&
                  agentName !== 'ServiceWorker' && agentName !== 'Security' &&
                  agentName !== 'Inspector' &&
                  agentName !== 'HeadlessExperimental' &&
                  agentName !== 'Fetch' && agentName !== 'Cast' &&
                  agentName !== 'BackgroundService';
            })
            .sort();

    async function disableAgent(agentName) {
      const agent = target.agents.get(agentName);
      const response = await agent.invoke_disable({});
      printResult(agentName, 'disable', response.getError());
    }

    async function enableAgent(agentName) {
      const agent = target.agents.get(agentName);
      const response = await agent.invoke_enable({});
      printResult(agentName, 'enable', response.getError());
    }

    for (const agentName of agentNames)
      await disableAgent(agentName);

    for (const agentName of agentNames) {
      await enableAgent(agentName);
      await disableAgent(agentName);
    }
  }

  TestRunner.completeTest();
})();
