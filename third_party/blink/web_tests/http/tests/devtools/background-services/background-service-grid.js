// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Application from 'devtools/panels/application/application.js';

function dumpBackgroundServiceGrid() {
  TestRunner.addResult('Grid Entries:');

    const treeElement = Application.ResourcesPanel.ResourcesPanel.instance().sidebar.backgroundFetchTreeElement;
  treeElement.onselect(false);

  const dataGrid = treeElement.view.dataGrid;
  if (!dataGrid.rootNode().children.length) {
    TestRunner.addResult('    [empty]');
    return;
  }

  for (const node of dataGrid.rootNode().children) {
    const children = Array.from(node.element().children).map(element => {
      if (!element.classList.contains('timestamp-column'))
        return element;
      // Format the timestamp column for consistent behavior.
      return {textContent: new Date(element.textContent).getTime()};
    });

    // Extract the text from the columns.
    const entries = Array.from(children, td => td.textContent).map(content => content ? content : '[blank]');
    TestRunner.addResult(' '.repeat(4) + entries.join(', '));
  }
};

function setOriginCheckbox(value) {
  const treeElement = Application.ResourcesPanel.ResourcesPanel.instance().sidebar.backgroundFetchTreeElement;
  treeElement.onselect(false);
  treeElement.view.originCheckbox.setChecked(value);
  // Simulate click.
  treeElement.view.refreshView();
}

(async function() {
  TestRunner.addResult(`Tests that the grid shows information as expected.\n`);
  await TestRunner.showPanel('resources');

  const backgroundServiceModel = TestRunner.mainTarget.model(Application.BackgroundServiceModel.BackgroundServiceModel);
  backgroundServiceModel.enable(Protocol.BackgroundService.ServiceName.BackgroundFetch);
  backgroundServiceModel.enable(Protocol.BackgroundService.ServiceName.BackgroundSync);

  // Grid is initially empty.
  dumpBackgroundServiceGrid();

  // Grid should have an entry now.
  backgroundServiceModel.backgroundServiceEventReceived({
    backgroundServiceEvent: {
      timestamp: 1556889085,  // 2019-05-03 14:11:25.000.
      origin: 'http://127.0.0.1:8000/',
      serviceWorkerRegistrationId: 42,  // invalid.
      service: Protocol.BackgroundService.ServiceName.BackgroundFetch,
      eventName: 'Event1',
      instanceId: 'Instance1',
      eventMetadata: [],
      storageKey: 'testKey',
    },
  });
  dumpBackgroundServiceGrid();

  // Event from a different service is ignored.
  backgroundServiceModel.backgroundServiceEventReceived({
    backgroundServiceEvent: {
      timestamp: 1556889085,  // 2019-05-03 14:11:25.000.
      origin: 'http://127.0.0.1:8000/',
      serviceWorkerRegistrationId: 42,  // invalid.
      service: Protocol.BackgroundService.ServiceName.BackgroundSync,
      eventName: 'Event1',
      instanceId: 'Instance2',
      eventMetadata: [],
      storageKey: 'testKey',
    },
  });
  dumpBackgroundServiceGrid();

  // Event from a different origin is ignored.
  backgroundServiceModel.backgroundServiceEventReceived({
    backgroundServiceEvent: {
      timestamp: 1556889085,  // 2019-05-03 14:11:25.000.
      origin: 'http://127.0.0.1:8080/',
      serviceWorkerRegistrationId: 42,  // invalid.
      service: Protocol.BackgroundService.ServiceName.BackgroundFetch,
      eventName: 'Event2',
      instanceId: 'Instance1',
      eventMetadata: [],
      storageKey: 'testKey',
    },
  });
  dumpBackgroundServiceGrid();

  // The event from a different origin should show up now.
  setOriginCheckbox(true);
  dumpBackgroundServiceGrid();

  // Unchecking the origin checkbox should remove it again.
  setOriginCheckbox(false);
  dumpBackgroundServiceGrid();

  // Simulate clicking the clear button.
  Application.ResourcesPanel.ResourcesPanel.instance().sidebar.backgroundFetchTreeElement.view.clearEvents();
  dumpBackgroundServiceGrid();

  TestRunner.completeTest();
})();
