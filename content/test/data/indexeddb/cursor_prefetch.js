// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These constants should match the ones in
// third_party/blink/renderer/modules/indexeddb/web_idb_cursor_impl.h to
// make sure the test hits the right code paths.
var kPrefetchThreshold = 2;
var kMinPrefetchAmount = 5;

var kNumberOfItems = 200;

function test() {
  indexedDBTest(setVersionSuccess, fillObjectStore);
}

function setVersionSuccess() {
  debug("setVersionSuccess():");
  window.db = event.target.result;
  window.trans = event.target.transaction;
  shouldBeTrue("trans !== null");
  var store = db.createObjectStore('store');
  store.createIndex('index', '');
}

function fillObjectStore() {
  debug("fillObjectStore()");
  var trans = db.transaction(['store'], 'readwrite');
  trans.onabort = unexpectedAbortCallback;
  trans.oncomplete = firstTest;

  var store = trans.objectStore('store');
  debug("Storing " + kNumberOfItems + " object in the object store.");
  for (var i = 0; i < kNumberOfItems; ++i) {
    var req = store.put(i, i);
    req.onerror = unexpectedErrorCallback;
  }

  // Let the transaction finish.
}

function firstTest() {
  debug("firstTest()");

  // Test iterating straight through the object store.

  var trans = db.transaction(['store'], 'readwrite');
  trans.onabort = unexpectedAbortCallback;
  trans.oncomplete = secondTest;

  var store = trans.objectStore('store');
  var cursorReq = store.openCursor();
  cursorReq.onerror = unexpectedErrorCallback;

  count = 0;
  cursorReq.onsuccess = function() {
    cursor = event.target.result;
    if (cursor === null) {
      shouldBe("count", "kNumberOfItems");
      return; // Let the transaction finish.
    }

    if (cursor.key !== count)
      shouldBe("cursor.key", "count");
    if (cursor.value !== count)
      shouldBe("cursor.value", "count");

    ++count;

    cursor.continue();
  };
}

function secondTest() {
  debug("secondTest()");

  // Test iterating through the object store, intermixed with
  // continue calls to specific keys.

  var trans = db.transaction(['store'], 'readwrite');
  trans.onabort = unexpectedAbortCallback;
  trans.oncomplete = thirdTest;

  var store = trans.objectStore('store');
  var cursorReq = store.openCursor();
  cursorReq.onerror = unexpectedErrorCallback;

  var jumpTable = [{from: 5,   to: 17},
                   {from: 25,  to: 30},
                   {from: 31,  to: 35},
                   {from: 70,  to: 80},
                   {from: 98,  to: 99}];

  count = 0;
  expectedKey = 0;

  cursorReq.onsuccess = function() {
    cursor = event.target.result;
    if (cursor === null) {
      debug("Finished iterating after " + count + " steps.");
      return; // Let the transaction finish.
    }

    if (cursor.key !== expectedKey)
      shouldBe("cursor.key", "expectedKey");
    if (cursor.value !== expectedKey)
      shouldBe("cursor.value", "expectedKey");

    ++count;

    for (var i = 0; i < jumpTable.length; ++i) {
      if (jumpTable[i].from === cursor.key) {
        expectedKey = jumpTable[i].to;
        debug("Jumping from "+ cursor.key + " to " + expectedKey);
        cursor.continue(expectedKey);
        return;
      }
    }

    ++expectedKey;
    cursor.continue();
  };
}

function thirdTest() {
  debug("thirdTest()");

  // Test iterating straight through the object store in reverse.

  var trans = db.transaction(['store'], 'readwrite');
  trans.onabort = unexpectedAbortCallback;
  trans.oncomplete = fourthTest;

  var store = trans.objectStore('store');
  var cursorReq = store.openCursor(
      IDBKeyRange.upperBound(kNumberOfItems-1), 'prev');
  cursorReq.onerror = unexpectedErrorCallback;

  count = 0;
  cursorReq.onsuccess = function() {
    cursor = event.target.result;
    if (cursor === null) {
      shouldBe("count", "kNumberOfItems");
      return; // Let the transaction finish.
    }

    expectedKey = kNumberOfItems - count - 1;

    if (cursor.key !== expectedKey)
      shouldBe("cursor.key", "expectedKey");
    if (cursor.value !== expectedKey)
      shouldBe("cursor.value", "expectedKey");

    ++count;

    cursor.continue();
  };
}

function fourthTest() {
  debug("fourthTest()");

  // Test iterating, and then stopping before reaching the end.
  // Make sure transaction terminates anyway.

  var trans = db.transaction(['store'], 'readwrite');
  trans.onabort = unexpectedAbortCallback;
  trans.oncomplete = function() {
    debug("fourthTest() transaction completed");
    fifthTest();
  };

  var store = trans.objectStore('store');
  var cursorReq = store.openCursor();
  cursorReq.onerror = unexpectedErrorCallback;

  count = 0;
  cursorReq.onsuccess = function() {
    cursor = event.target.result;

    if (cursor.key !== count)
      shouldBe("cursor.key", "count");
    if (cursor.value !== count)
      shouldBe("cursor.value", "count");

    ++count;

    if (count === 25) {
      // Schedule some other request.
      var otherReq = store.get(42);
      otherReq.onerror = unexpectedErrorCallback;
      otherReq.onsuccess = function() {
        if (count === 25) {
          debug("Other request fired before continue, as expected.");
        } else {
          debug("Other request fired out-of-order!");
          fail();
        }
      };

      cursor.continue();
      return;
    }

    if (count === 30) {
      // Do a continue first, then another request.
      cursor.continue();

      var otherReq = store.get(42);
      otherReq.onerror = unexpectedErrorCallback;
      otherReq.onsuccess = function() {
        if (count === 31) {
          debug("Other request fired right after continue as expected.");
        } else {
          debug("Other request didn't fire right after continue as expected.");
          fail();
        }
      };

      return;
    }

    if (count === 75) {
      return;  // Sudden stop.
    }

    cursor.continue();
  };
}

function fifthTest() {
  debug("fifthTest()");

  // Test iterating over the pre-fetch threshold, but make sure the
  // cursor is positioned so that it is actually at the last element
  // in the range when pre-fetch fires, and make sure a null cursor
  // is the result as expected.

  var trans = db.transaction(['store'], 'readwrite');
  trans.onabort = unexpectedAbortCallback;
  trans.oncomplete = sixthTest;

  var store = trans.objectStore('store');

  var startKey = kNumberOfItems - 1 - kPrefetchThreshold;
  var cursorReq = store.openCursor(IDBKeyRange.lowerBound(startKey));
  cursorReq.onerror = unexpectedErrorCallback;

  count = 0;
  cursorReq.onsuccess = function() {
    cursor = event.target.result;

    if (cursor === null) {
      debug("cursor is null");
      shouldBe("count", "kPrefetchThreshold + 1");
      return;
    }

    debug("count: " + count);
    ++count;
    cursor.continue();
  };
}

function sixthTest() {
  debug("sixthTest()");

  // Test stepping two cursors simultaneously. First cursor1 steps
  // for a while, then cursor2, then back to cursor1, etc.

  var trans = db.transaction(['store'], 'readwrite');
  trans.onabort = unexpectedAbortCallback;
  trans.oncomplete = seventhTest;
  var store = trans.objectStore('store');

  cursor1 = null;
  cursor2 = null;

  count1 = 0;
  count2 = 0;

  var cursor1func = function() {
    var cursor = event.target.result;
    if (cursor === null) {
      shouldBe("count1", "kNumberOfItems");
      cursor2.continue();
      return;
    }

    if (cursor1 === null) {
      cursor1 = cursor;
    }

    if (cursor1.key !== count1)
      shouldBe("cursor1.key", "count1");
    if (cursor1.value !== count1)
      shouldBe("cursor1.value", "count1");

    ++count1;

    if (count1 % 20 === 0) {
      if (cursor2 !== null) {
        cursor2.continue();
      } else {
        var req = store.openCursor();
        req.onerror = unexpectedErrorCallback;
        req.onsuccess = cursor2func;
      }
    } else {
      cursor1.continue();
    }
  };

  var cursor2func = function() {
    var cursor = event.target.result;
    if (cursor === null) {
      shouldBe("count2", "kNumberOfItems");
      return;
    }

    if (cursor2 === null) {
      cursor2 = cursor;
    }

    if (cursor2.key !== count2)
      shouldBe("cursor2.key", "count2");
    if (cursor2.value !== count2)
      shouldBe("cursor2.value", "count2");

    ++count2;

    if (count2 % 20 === 0) {
      cursor1.continue();
    } else {
      cursor2.continue();
    }
  };

  var req = store.openCursor();
  req.onerror = unexpectedErrorCallback;
  req.onsuccess = cursor1func;
}

function seventhTest() {
  debug("seventhTest()");

  // Test iterating straight through an index.

  var trans = db.transaction(['store'], 'readwrite');
  trans.onabort = unexpectedAbortCallback;
  trans.oncomplete = eighthTest;

  var store = trans.objectStore('store');
  var index = store.index('index');

  var cursorReq = index.openCursor();
  cursorReq.onerror = unexpectedErrorCallback;
  count = 0;

  cursorReq.onsuccess = function() {
    cursor = event.target.result;
    if (cursor === null) {
      shouldBe("count", "kNumberOfItems");
      return;
    }

    if (cursor.key !== count)
      shouldBe("cursor.key", "count");
    if (cursor.primaryKey !== count)
      shouldBe("cursor.primaryKey", "count");
    if (cursor.value !== count)
      shouldBe("cursor.value", "count");

    ++count;
    cursor.continue();
  };
}

function eighthTest() {
  debug("eighthTest()");

  // Run a key cursor over an index.

  var trans = db.transaction(['store'], 'readwrite');
  trans.onabort = unexpectedAbortCallback;
  trans.oncomplete = done;

  var store = trans.objectStore('store');
  var index = store.index('index');

  var cursorReq = index.openKeyCursor();
  cursorReq.onerror = unexpectedErrorCallback;
  count = 0;

  cursorReq.onsuccess = function() {
    cursor = event.target.result;
    if (cursor === null) {
      shouldBe("count", "kNumberOfItems");
      return;
    }

    if (cursor.key !== count)
      shouldBe("cursor.key", "count");
    if (cursor.primaryKey !== count)
      shouldBe("cursor.primaryKey", "count");

    ++count;
    cursor.continue();
  };
}
