// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests RemoteObject.getProperties on localStorage object. 66215\n`);
  await TestRunner.evaluateInPagePromise(`
      localStorage.testProperty = "testPropertyValue";
  `);

  var result = await TestRunner.RuntimeAgent.evaluate('localStorage');
  var localStorageHandle = TestRunner.runtimeModel.createRemoteObject(result);
  localStorageHandle.getOwnProperties(false).then(step2);

  function step2(allProperties) {
    const properties = allProperties.properties;
    for (var property of properties) {
      if (property.name !== 'testProperty')
        continue;
      property.value = {type: property.value.type, description: property.value.description};
      TestRunner.dump(property);
    }
    TestRunner.completeTest();
  }
})();
