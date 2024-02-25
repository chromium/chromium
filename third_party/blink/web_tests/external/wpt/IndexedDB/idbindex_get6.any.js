// META: global=window,worker
// META: title=IDBIndex.get() - throw InvalidStateError when the index is deleted
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
    store.deleteIndex("index");

    assert_throws_dom("InvalidStateError", function(){
        index.get("data");
    });
    t.done();
}
