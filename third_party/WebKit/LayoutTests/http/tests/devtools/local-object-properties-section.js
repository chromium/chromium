// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that ObjectPropertiesSection works with local remote objects.\n`);
  await TestRunner.loadModule('object_ui');

  var d = [];
  for (var i = 1000; i < 1256; ++i)
    d.push(i);
  var object = {a: 'b', c: d};
  var localObject = SDK.RemoteObject.fromLocalObject(object);

  var propertiesSection = new ObjectUI.ObjectPropertiesSection(localObject, 'local object');
  propertiesSection.expand();
  await new Promise(resolve => setTimeout(resolve, 0));
  propertiesSection.objectTreeElement().childAt(1).expand();
  await new Promise(resolve => setTimeout(resolve, 0));

  TestRunner.addResult(TestRunner.textContentWithLineBreaks(propertiesSection.element));
  TestRunner.completeTest();
})();
