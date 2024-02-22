// META: global=window,worker
// META: title=IDBIndex.get() - throw TransactionInactiveError on aborted transaction
// META: script=resources/support.js
// @author Intel <http://www.intel.com>

'use_strict';

let db;
const t = async_test();

const open_rq = createdb(t);
open_rq.onupgradeneeded = function(e) {
    db = e.target.result;
    const store = db.createObjectStore("store", { keyPath: "key" });
    const index = store.createIndex("index", "indexedProperty");
    store.add({ key: 1, indexedProperty: "data" });
}
open_rq.onsuccess = function(e) {
    db = e.target.result;
    const tx = db.transaction('store', 'readonly', {durability: 'relaxed'});
    const index = tx.objectStore('store').index('index');
    tx.abort();

    assert_throws_dom("TransactionInactiveError", function(){
        index.get("data");
    });
    t.done();
}
