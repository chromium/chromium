if (this.importScripts) {
    importScripts('../../../resources/gc.js');
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Ensure that IDBDatabase objects are deleted when there are no retaining paths left");

indexedDBTest(prepareDatabase, openSuccess);
function prepareDatabase()
{
}

function setVersion()
{
    debug("Open request should not receive a blocked event:");
    var request = evalAndLog("indexedDB.open(dbname, 2)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = finishJSTest;
}

function openSuccess()
{
    db = event.target.result;
    evalAndLog("db.close()");

    var openRequest = evalAndLog("indexedDB.open(dbname)");
    openRequest.onblocked = unexpectedBlockedCallback;
    openRequest.onupgradeneeded = unexpectedUpgradeNeededCallback;
    openRequest.onerror = unexpectedErrorCallback;
    openRequest.onsuccess = function() {
        debug("Dropping references to new connection.");
        // After leaving this function, there are no remaining references to the
        // db, so it should get deleted.
        setTimeout(function () {
            asyncGC(setVersion);
        }, 2);
    };
}
