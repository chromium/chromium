// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests RemoteObject.getProperties.\n`);
  await TestRunner.evaluateInPagePromise(`
      var object1 = { get foo() { return 1; }, set foo(value) { } };
      var object2 = { get foo() { return 1; } };
  `);

  var obj1, obj2;

  function dumpProperties(allProperties) {
    const properties = allProperties.properties;
    for (var i = 0; i < properties.length; ++i)
      dumpProperty(properties[i]);
  }

  TestRunner.runTestSuite([
    async function testSetUp(next) {
      await new Promise(resolve => TestRunner.evaluateInPage('dumpObjects(\'Initial\')', resolve));

      var result = await TestRunner.RuntimeAgent.evaluate('object1');

      obj1 = TestRunner.runtimeModel.createRemoteObject(result);
      var result = await TestRunner.RuntimeAgent.evaluate('object2');

      obj2 = TestRunner.runtimeModel.createRemoteObject(result);
      next();
    },

    function testGetterAndSetter(next) {
      obj1.getOwnProperties(false).then(dumpProperties).then(next);
    },

    function testGetterOnly(next) {
      obj2.getOwnProperties(false).then(dumpProperties).then(next);
    }
  ]);

  function convertPropertyValueForTest(propertyObject, fieldName) {
    var value = propertyObject[fieldName];
    if (value)
      propertyObject[fieldName] = {
        type: value.type,
        description: value.description.replace(/^function [gs]et foo/, 'function '),
        objectId: value.objectId
      };
  }

  function dumpProperty(property) {
    if (property.name === '__proto__')
      return;

    convertPropertyValueForTest(property, 'value');
    convertPropertyValueForTest(property, 'getter');
    convertPropertyValueForTest(property, 'setter');
    TestRunner.dump(property, {objectId: 'formatAsTypeName'});
  }
})();
