// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console dumps global object with properties.\n`);

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    function doit()
    {
        console.dir(window);
    };
  `);

  TestRunner.RuntimeAgent.evaluate('window', 'console', false).then(evalCallback);

  function evalCallback(result) {
    if (!result) {
      testController.notifyDone('Exception');
      return;
    }

    if (result.type === 'error')
      testController.notifyDone('Exception:' + result);

    var objectProxy = TestRunner.runtimeModel.createRemoteObject(result);
    objectProxy.getOwnProperties(false).then(getPropertiesCallback);
  }

  function getPropertiesCallback(allProperties) {
    const properties = allProperties.properties;
    properties.sort(ObjectUI.ObjectPropertiesSection.CompareProperties);

    var golden = {
      'window': 1,
      'document': 1,
      'eval': 1,
      'console': 1,
      'frames': 1,
      'Array': 1,
      'doit': 1
    };

    var result = {};

    for (var i = 0; i < properties.length; ++i) {
      var name = properties[i].name;

      if (golden[name])
        result[name] = 1;
    }

    TestRunner.addObject(result);
    TestRunner.completeTest();
  }
})();
