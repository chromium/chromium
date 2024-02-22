// META: global=window,worker
// META: title=IDBIndex.get() - attempt to retrieve a record that doesn't exist
// META: script=resources/support.js
// @author Microsoft <https://www.microsoft.com>

'use_strict';

let db;
const t = async_test();

const open_rq = createdb(t);
open_rq.onupgradeneeded = function(e) {
    db = e.target.result;
    const rq = db.createObjectStore("test", { keyPath: "key" })
                .createIndex("index", "indexedProperty")
                .get(1);

    rq.onsuccess = t.step_func(function(e) {
        assert_equals(e.target.result, undefined);
        t.done();
    });
};
