// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class DatabaseUtils {
  /** Wrap an IndexedDB request into a Promise.
   *
   * This should not be used for open().
   *
   * @param {IDBRequest} request the request to be wrapped
   * @returns {Promise<Object>} promise that resolves with the request's result,
   *     or rejects with an error
   */
  static async promiseForRequest(request) {
    return new Promise((resolve, reject) => {
      request.onsuccess = event => { resolve(event.target.result); };
      request.onerror = event => { reject(event.target.error); };
      request.onblocked = event => {
        reject(event.target.error ||
                (new Error("blocked by other database connections")));
      };
      request.onupgradeneeded = event => {
        reject(event.target.error ||
                (new Error("unexpected upgradeneeded event")));
      };
    });
  }

  /** Wrap an IndexedDB database open request into a Promise.
   *
   * This is intended to be used by open().
   *
   * @param {IDBOpenDBRequest} request the request to be wrapped
   * @returns {Promise<{database: idbDatabase, transaction: IDBTransaction?}>}
   *     promise that resolves with an object whose "database" property is the
   *     newly opened database; if an upgradeneeded event is received, the
   *     "transaction" property holds the upgrade transaction
   */
  static async promiseForOpenRequest(request) {
    return new Promise((resolve, reject) => {
      request.onsuccess = event => {
        resolve({ database: event.target.result, transaction: null });
      };
      request.onerror = event => { reject(event.target.error); };
      request.onblocked = event => {
        reject(event.target.error ||
                (new Error("blocked by other database connections")));
      };
      request.onupgradeneeded = event => {
        resolve({
          database: event.target.result,
          transaction: event.target.transaction
        });
      };
    });
  }

  /** Wrap an IndexedDB transaction into a Promise.
   *
   * @param {IDBTransaction} transaction the transaction to be wrapped
   * @returns {Promise<Object>} promise that resolves with undefined when the
   *     transaction is completed, or rejects with an error if the transaction
   *     is aborted or errors out
   */
  static async promiseForTransaction(transaction) {
    return new Promise((resolve, reject) => {
      transaction.oncomplete = () => { resolve(); };
      transaction.onabort = event => { reject(event.target.error); };
      transaction.onerror = event => { reject(event.target.error); };
    });
  }
}

class Database {
  /** Open a database.
   *
   * @param {string} dbName the name of the database to be opened
   * @return {Promise<Database>} promise that resolves with a new Database
   *     instance for the open database
   */
  static async open(dbName) {
    const request = indexedDB.open(dbName, 1);
    const result = await DatabaseUtils.promiseForOpenRequest(request);

    if (result.transaction !== null) {
      fail("expected db to exist");
      return;
    }

    return new Database(dbName, result.database);
  }

  /** Do not instantiate directly. Use Database.open() instead.
   *
   * @param {string} dbName the database's name
   * @param {IDBDatabase} idbDatabase the IndexedDB instance wrapped by this
   */
  constructor(dbName, idbDatabase) {
    this._dbName = dbName;
    this._idbDatabase = idbDatabase;
  }

  /** Closes the underlying database. All future operations will fail. */
  close() {
    this._idbDatabase.close();
  }

  /** Reads from a store by iterating a cursor.
   *
   * @param {string} storeName the name of the store being read
   * @param {{index?: string, range?: IDBKeyRange}} query narrows down the data
   *     being read
   * @param {function(IDBCursor): boolean} cursorCallback called for each cursor
   *     yielded by the iteration; must return a truthy value to continue
   *     iteration, or a falsey value to stop iterating
   */
  async iterateCursor(storeName, query, cursorCallback) {
    const transaction = this._idbDatabase.transaction([storeName], 'readonly');
    const transactionPromise = DatabaseUtils.promiseForTransaction(transaction);

    const objectStore = transaction.objectStore(storeName);
    const dataSource = ('index' in query) ? objectStore.index(query.index)
                                          : objectStore;
    const request = ('range' in query) ? dataSource.openCursor(query.range)
                                        : dataSource.openCursor();
    while (true) {
      const cursor = await DatabaseUtils.promiseForRequest(request);
      if (!cursor)
        break;  // The iteration completed.

      const willContinue = cursorCallback(cursor);
      if (!willContinue)
        break;
      cursor.continue();
    }

    await transactionPromise;
    return true;
  }

  async read() {
    const status = document.getElementById('status');
    status.textContent = 'Started reading items';

    let i = 0;
    const result = await this.iterateCursor('store', {}, cursor => {
      i += 1;
      if (cursor.primaryKey !== i) {
        status.textContent =
            `Incorrect primaryKey - wanted ${i} got ${cursor.primaryKey}`;
        return false;
      }
      if (cursor.key !== i) {
        status.textContent = `Incorrect key - wanted ${i} got ${cursor.key}`;
        return false;
      }
      if (cursor.value.id !== i) {
        status.textContent =
            `Incorrect value.id - wanted ${i} got ${cursor.key}`;
        return false;
      }
      return true;
    });
    if (!result) {
      status.textContent = `Failed to read items`;
      return false;
    }
    status.textContent = `Done reading ${i} items`;
    return true;
  }
};
