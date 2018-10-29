// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests callFunction on local remote objects.\n`);

  var object = [6, 28, 496];
  var localObject = SDK.RemoteObject.fromLocalObject(object);

  function getItem(index) {
    return this[index];
  }

  let result = await localObject.callFunctionJSON(getItem, [{value: 1}]);
  TestRunner.addResult('getItem(1) result: ' + result);

  function compareAndSwap(index, value, newValue) {
    if (this[index] !== value)
      throw 'Data corrupted';
    this[index] = newValue;
    return 'Done';
  }

  result = await localObject.callFunction(
      compareAndSwap, [{value: 1}, {value: 28}, {value: 42}]);
  TestRunner.addResult(
      'compareAndSwap(1, 28, 42) result: ' + result.object.description);
  result = await localObject.callFunction(
      compareAndSwap, [{value: 1}, {value: 28}, {value: 42}]);
  TestRunner.addResult(
      'compareAndSwap(1, 28, 42) throws exception: ' + !!result.wasThrown);

  function guessWhat() {
    return 42;
  }

  result = await localObject.callFunction(guessWhat, undefined);
  TestRunner.addResult('guessWhat() result: ' + result.object.description);

  await localObject.callFunction(
      compareAndSwap, [{value: 0}, {value: 6}, {value: 7}]);
  TestRunner.addResult('Final value of object: [' + object.join(', ') + ']');
  TestRunner.completeTest();
})();
