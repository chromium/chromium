// META: global=window,worker
// META: title=IDBIndex.get() - returns the record
// META: script=resources/support.js
// @author Microsoft <https://www.microsoft.com>

'use_strict';

let db;
let index;
const t = async_test(),
    record = { key: 1, indexedProperty: "data" };

const open_rq = createdb(t);
open_rq.onupgradeneeded = function(e) {
    db = e.target.result;
    const objStore = db.createObjectStore("store", { keyPath: "key" });
    index = objStore.createIndex("index", "indexedProperty");

    objStore.add(record);
}

open_rq.onsuccess = function(e) {
    const rq = db.transaction("store", "readonly", {durability: 'relaxed'})
                .objectStore("store")
                .index("index")
                .get(record.indexedProperty);

    rq.onsuccess = t.step_func(function(e) {
        assert_equals(e.target.result.key, record.key);
        t.done();
    });
}
