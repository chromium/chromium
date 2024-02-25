// META: global=window,worker
// META: title=IDBIndex.get() - throw InvalidStateError on index deleted by aborted upgrade
// META: script=resources/support.js

'use_strict';

let db;
const t = async_test();

const open_rq = createdb(t);
open_rq.onupgradeneeded = function(e) {
    db = e.target.result;
    const store = db.createObjectStore("store", { keyPath: "key" });
    const index = store.createIndex("index", "indexedProperty");
    store.add({ key: 1, indexedProperty: "data" });

    e.target.transaction.abort();

    assert_throws_dom("InvalidStateError", function(){
        index.get("data");
    });
    t.done();
}
