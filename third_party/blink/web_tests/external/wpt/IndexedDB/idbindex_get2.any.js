// META: global=window,worker
// META: title=IDBIndex.get() - returns the record where the index contains duplicate values
// META: script=resources/support.js
// @author Microsoft <https://www.microsoft.com>

'use_strict';

let db;
const t = async_test();
const records = [ { key:1, indexedProperty:"data" },
                { key:2, indexedProperty:"data" },
                { key:3, indexedProperty:"data" } ];

const open_rq = createdb(t);
open_rq.onupgradeneeded = function(e) {
    db = e.target.result;
    const objStore = db.createObjectStore("test", { keyPath: "key" });
    objStore.createIndex("index", "indexedProperty");

    for (let i = 0; i < records.length; i++)
        objStore.add(records[i]);
};

open_rq.onsuccess = function(e) {
    const rq = db.transaction("test", "readonly", {durability: 'relaxed'})
                .objectStore("test")
                .index("index")
                .get("data");

    rq.onsuccess = t.step_func(function(e) {
        assert_equals(e.target.result.key, records[0].key);
        t.done();
    });
};
