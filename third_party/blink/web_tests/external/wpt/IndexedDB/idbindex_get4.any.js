// META: global=window,worker
// META: title=IDBIndex.get() - returns the record with the first key in the range
// META: script=resources/support.js
// @author Microsoft <https://www.microsoft.com>

'use_strict';

let db;
const t = async_test();

const open_rq = createdb(t);

open_rq.onupgradeneeded = function(e) {
    db = e.target.result;
    const store = db.createObjectStore("store", { keyPath: "key" });
    store.createIndex("index", "indexedProperty");

    for(let i = 0; i < 10; i++) {
        store.add({ key: i, indexedProperty: "data" + i });
    }
}

open_rq.onsuccess = function(e) {
    const rq = db.transaction("store", "readonly", {durability: 'relaxed'})
                .objectStore("store")
                .index("index")
                .get(IDBKeyRange.bound('data4', 'data7'));

    rq.onsuccess = t.step_func(function(e) {
        assert_equals(e.target.result.key, 4);
        assert_equals(e.target.result.indexedProperty, 'data4');

        step_timeout(function() { t.done(); }, 4);
    });
}
