// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as ObjectUI from 'devtools/ui/legacy/components/object_ui/object_ui.js';
import * as SDK from 'devtools/core/sdk/sdk.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Test that ObjectPropertiesSection works with local remote objects.\n`);

  var d = [];
  for (var i = 1000; i < 1256; ++i)
    d.push(i);
  var object = {a: 'b', c: d};
  var localObject = SDK.RemoteObject.RemoteObject.fromLocalObject(object);

  var propertiesSection = new ObjectUI.ObjectPropertiesSection.ObjectPropertiesSectionWidget();
  propertiesSection.markAsRoot();
  propertiesSection.root = localObject;
  const titleSpan = document.createElement('span');
  titleSpan.textContent = 'local object';
  propertiesSection.title = titleSpan;
  propertiesSection.show(document.body);
  propertiesSection.objectTree.expanded = true;
  await UI.Widget.Widget.allUpdatesComplete;

  const treeOutline = propertiesSection.element.querySelector('devtools-tree')?.getInternalTreeOutlineForTest();
  const rootElement = treeOutline.firstChild();
  await rootElement.childAt(1).expand();
  await UI.Widget.Widget.allUpdatesComplete;

  TestRunner.addResult(TestRunner.textContentWithLineBreaks(propertiesSection.element));
  TestRunner.completeTest();
})();
