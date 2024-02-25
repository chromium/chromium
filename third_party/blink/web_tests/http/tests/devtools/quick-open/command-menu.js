// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as QuickOpen from 'devtools/ui/legacy/components/quick_open/quick_open.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Test that the command menu is properly filled.\n`);

  var categories = new Set();
  var commands = new Map();
  QuickOpen.CommandMenu.CommandMenu.instance().commands().forEach(command => {
    categories.add(command.category);
    commands.set(command.category + ': ' + command.title, command);
  });

  TestRunner.addResult('Categories active:');
  Array.from(categories).sort().forEach(category => TestRunner.addResult('Has category: ' + category));

  TestRunner.addResult('');
  const expectedCommands = [
    'Panel: Show Console', 'Drawer: Show Console', 'Appearance: Switch to dark theme',
    'Global: Auto-open DevTools for popups'
  ];
  expectedCommands.forEach(item => {
    if (!commands.has(item))
      TestRunner.addResult(item + ' is MISSING');
  });

  TestRunner.addResult('Switching to console panel');
  try {
    commands.get('Panel: Show Console').execute().then(() => {
      TestRunner.addResult('Current panel: ' + UI.InspectorView.InspectorView.instance().currentPanelDeprecated().name);
      TestRunner.completeTest();
    });
  } catch (e) {
    TestRunner.addResult(e);
    TestRunner.completeTest();
  }
})();
