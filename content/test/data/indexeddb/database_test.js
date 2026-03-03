// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

async function test() {
  const db = await promiseOpenDb('dbname', (database) => {
    debug('Populating object store');
    const objectStore =
        database.createObjectStore('employees', {keyPath: 'id'});
    if (objectStore.name !== 'employees') {
      fail(`Expected objectStore.name to be 'employees', got '${
          objectStore.name}'`);
    }
    if (objectStore.keyPath !== 'id') {
      fail(`Expected objectStore.keyPath to be 'id', got '${
          objectStore.keyPath}'`);
    }

    if (database.name !== 'dbname') {
      fail(`Expected database.name to be 'dbname', got '${database.name}'`);
    }
    if (database.version !== 1) {
      fail(`Expected database.version to be 1, got '${database.version}'`);
    }
    if (database.objectStoreNames.length !== 1) {
      fail(`Expected objectStoreNames.length to be 1, got '${
          database.objectStoreNames.length}'`);
    }
    if (database.objectStoreNames[0] !== 'employees') {
      fail(`Expected objectStoreNames[0] to be 'employees', got '${
          database.objectStoreNames[0]}'`);
    }

    debug('Deleting an object store.');
    database.deleteObjectStore('employees');
    if (database.objectStoreNames.length !== 0) {
      fail(`Expected objectStoreNames.length to be 0, got '${
          database.objectStoreNames.length}'`);
    }
  });

  done();
}
