// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function cursorSuccess()
{
    debug("Cursor opened successfully.");
    // FIXME: check that we can iterate the cursor.
    shouldBe("event.target.result.direction", "'next'");
    shouldBe("event.target.result.key", "'myKey' + count");
    shouldBe("event.target.result.value.keyPath", "'myKey' + count");
    shouldBe("event.target.result.value.value", "'myValue' + count");
    if (++count >= 5)
        done();
    else
        openCursor();
}

function openCursor()
{
    debug("Opening cursor #" + count);
    keyRange = IDBKeyRange.lowerBound("myKey" + count);
    request = objectStore.openCursor(keyRange);
    request.onsuccess = cursorSuccess;
    request.onerror = unexpectedErrorCallback;
}

function populateObjectStore()
{
    debug("Populating object store #" + count);
    obj = {'keyPath': 'myKey' + count, 'value': 'myValue' + count};
    request = objectStore.add(obj);
    request.onerror = unexpectedErrorCallback;
    if (++count >= 5) {
        count = 0;
        request.onsuccess = openCursor;
    } else {
        request.onsuccess = populateObjectStore;
    }
}

function createObjectStore()
{
    debug('createObjectStore');
    db = event.target.result;
    window.objectStore = db.createObjectStore('test', {keyPath: 'keyPath'});
    count = 0;
    populateObjectStore();
}

function test()
{
    indexedDBTest(createObjectStore);
}
