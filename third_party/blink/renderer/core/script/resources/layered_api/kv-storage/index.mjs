// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createStorageAreaAsyncIterator} from './async_iterator.mjs';
import {promiseForRequest, promiseForTransaction, throwForDisallowedKey} from './idb_utils.mjs';

// Overall TODOs/spec-noncompliances:
// - Susceptible to tampering of built-in prototypes and globals. We want to
//   work on tooling to ameliorate that.

const DEFAULT_STORAGE_AREA_NAME = 'default';
const DEFAULT_IDB_STORE_NAME = 'store';

// TODO(crbug.com/977470): this should be handled via infrastructure that
// avoids putting it in the module map entirely, not as a runtime check.
// Several web platform tests fail because of this.
if (!self.isSecureContext) {
  throw new TypeError('KV Storage is only available in secure contexts');
}

export class StorageArea {
  #backingStoreObject;
  #databaseName;
  #databasePromise;

  // TODO: Once private methods land in Chrome, this private field can
  // be refactored out with a private static method.
  #setPromise = promise => {
    this.#databasePromise = promise;
  };

  constructor(name) {
    this.#databaseName = `kv-storage:${name}`;
  }

  async set(key, value) {
    throwForDisallowedKey(key);

    return performDatabaseOperation(
        this.#databasePromise, this.#setPromise, this.#databaseName,
        'readwrite', (transaction, store) => {
          if (value === undefined) {
            store.delete(key);
          } else {
            store.put(value, key);
          }

          return promiseForTransaction(transaction);
        });
  }

  async get(key) {
    throwForDisallowedKey(key);

    return performDatabaseOperation(
        this.#databasePromise, this.#setPromise, this.#databaseName, 'readonly',
        (transaction, store) => {
          return promiseForRequest(store.get(key));
        });
  }

  async delete(key) {
    throwForDisallowedKey(key);

    return performDatabaseOperation(
        this.#databasePromise, this.#setPromise, this.#databaseName,
        'readwrite', (transaction, store) => {
          store.delete(key);
          return promiseForTransaction(transaction);
        });
  }

  async clear() {
    if (!this.#databasePromise) {
      // Don't try to delete, and clear the promise, while we're opening the
      // database; wait for that first.
      try {
        await this.#databasePromise;
      } catch {
        // If the database failed to initialize, then that's fine, we'll still
        // try to delete it.
      }

      this.#databasePromise = undefined;
    }

    return promiseForRequest(self.indexedDB.deleteDatabase(this.#databaseName));
  }

  keys() {
    // Brand check: throw if there is no such private field.
    // eslint-disable-next-line no-unused-expressions
    this.#databaseName;

    return createStorageAreaAsyncIterator(
        'keys',
        steps => performDatabaseOperation(
            this.#databasePromise, this.#setPromise, this.#databaseName,
            'readonly', steps));
  }

  values() {
    // Brand check: throw if there is no such private field.
    // eslint-disable-next-line no-unused-expressions
    this.#databaseName;

    return createStorageAreaAsyncIterator(
        'values',
        steps => performDatabaseOperation(
            this.#databasePromise, this.#setPromise, this.#databaseName,
            'readonly', steps));
  }

  entries() {
    // Brand check: throw if there is no such private field.
    // eslint-disable-next-line no-unused-expressions
    this.#databaseName;

    return createStorageAreaAsyncIterator(
        'entries',
        steps => performDatabaseOperation(
            this.#databasePromise, this.#setPromise, this.#databaseName,
            'readonly', steps));
  }

  get backingStore() {
    if (!this.#backingStoreObject) {
      this.#backingStoreObject = Object.freeze({
        database: this.#databaseName,
        store: DEFAULT_IDB_STORE_NAME,
        version: 1,
      });
    }

    return this.#backingStoreObject;
  }
}

StorageArea.prototype[Symbol.asyncIterator] = StorageArea.prototype.entries;
StorageArea.prototype[Symbol.toStringTag] = 'StorageArea';

// Override the defaults that are implied by using class declarations and
// assignment, to be more Web IDL-ey.
// https://github.com/heycam/webidl/issues/738 may modify these a bit.
Object.defineProperties(StorageArea.prototype, {
  set: {enumerable: true},
  get: {enumerable: true},
  delete: {enumerable: true},
  clear: {enumerable: true},
  keys: {enumerable: true},
  values: {enumerable: true},
  entries: {enumerable: true},
  backingStore: {enumerable: true},
  [Symbol.asyncIterator]: {enumerable: false},
  [Symbol.toStringTag]: {writable: false, enumerable: false},
});

export default new StorageArea(DEFAULT_STORAGE_AREA_NAME);

async function performDatabaseOperation(
    promise, setPromise, name, mode, steps) {
  if (!promise) {
    promise = initializeDatabasePromise(setPromise, name);
  }

  const database = await promise;
  const transaction = database.transaction(DEFAULT_IDB_STORE_NAME, mode);
  const store = transaction.objectStore(DEFAULT_IDB_STORE_NAME);

  const result = steps(transaction, store);
  transaction.commit();
  return result;
}

function initializeDatabasePromise(setPromise, databaseName) {
  const promise = new Promise((resolve, reject) => {
    const request = self.indexedDB.open(databaseName, 1);

    request.onsuccess = () => {
      const database = request.result;

      if (!checkDatabaseSchema(database, databaseName, reject)) {
        return;
      }

      database.onclose = () => setPromise(undefined);
      database.onversionchange = () => {
        database.close();
        setPromise(undefined);
      };
      resolve(database);
    };

    request.onerror = () => reject(request.error);

    request.onupgradeneeded = () => {
      try {
        request.result.createObjectStore(DEFAULT_IDB_STORE_NAME);
      } catch (e) {
        reject(e);
      }
    };
  });
  setPromise(promise);
  return promise;
}

function checkDatabaseSchema(database, databaseName, reject) {
  if (database.objectStoreNames.length !== 1) {
    reject(new DOMException(
        `KV storage database "${databaseName}" corrupted: there are ` +
            `${database.objectStoreNames.length} object stores, instead of ` +
            `the expected 1.`,
        'InvalidStateError'));
    return false;
  }

  if (database.objectStoreNames[0] !== DEFAULT_IDB_STORE_NAME) {
    reject(new DOMException(
        `KV storage database "${databaseName}" corrupted: the object store ` +
            `is named "${database.objectStoreNames[0]}" instead of the ` +
            `expected "${DEFAULT_IDB_STORE_NAME}".`,
        'InvalidStateError'));
    return false;
  }

  const transaction = database.transaction(DEFAULT_IDB_STORE_NAME, 'readonly');
  const store = transaction.objectStore(DEFAULT_IDB_STORE_NAME);

  if (store.autoIncrement !== false || store.keyPath !== null ||
      store.indexNames.length !== 0) {
    reject(new DOMException(
        `KV storage database "${databaseName}" corrupted: the ` +
            `"${DEFAULT_IDB_STORE_NAME}" object store has a non-default ` +
            `schema.`,
        'InvalidStateError'));
    return false;
  }

  return true;
}
