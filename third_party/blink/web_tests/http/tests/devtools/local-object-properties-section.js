// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as ObjectUI from 'devtools/ui/legacy/components/object_ui/object_ui.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Test that ObjectPropertiesSection works with local remote objects.\n`);

  var d = [];
  for (var i = 1000; i < 1256; ++i)
    d.push(i);
  var object = {a: 'b', c: d};
  var localObject = SDK.RemoteObject.RemoteObject.fromLocalObject(object);

  var propertiesSection = new ObjectUI.ObjectPropertiesSection.ObjectPropertiesSection(localObject, 'local object');
  propertiesSection.expand();
  await new Promise(resolve => setTimeout(resolve, 0));
  propertiesSection.objectTreeElement().childAt(1).expand();
  await new Promise(resolve => setTimeout(resolve, 0));

  TestRunner.addResult(TestRunner.textContentWithLineBreaks(propertiesSection.element));
  TestRunner.completeTest();
})();
