// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`The test verifies that DevTools events work.\n`);
  var object = new Common.ObjectWrapper.ObjectWrapper();
  var eventSymbol = Symbol('Event');

  addListener('original listener');
  dispatch('first event');
  addListener('second listener');
  dispatch('second event');
  removeListener('second listener');
  dispatch('third event');

  TestRunner.addResult('Adding a listener that removes a later listener')
  object.addEventListener(eventSymbol, event => {
    TestRunner.addResult(`removing the listener during the event: ${event.data} `);
    removeListener('later listener to be removed');
  });
  addListener('later listener to be removed')
  dispatch('fourth event');

  TestRunner.completeTest();

  function eventListener(event) {
    TestRunner.addResult(`Heard event with the data '${event.data}' and this '${this}'`);
  }

  function dispatch(data) {
    TestRunner.addResult(`Dispatching event with the data '${data}'`);
    object.dispatchEventToListeners(eventSymbol, data);
    TestRunner.addResult('');
  }

  function addListener(thisValue) {
    TestRunner.addResult(`Adding a listener with this '${thisValue}'`);
    object.addEventListener(eventSymbol, eventListener, thisValue);
  }

  function removeListener(thisValue) {
    TestRunner.addResult(`Removing a listener with this '${thisValue}'`);
    object.removeEventListener(eventSymbol, eventListener, thisValue);

  }

})();
