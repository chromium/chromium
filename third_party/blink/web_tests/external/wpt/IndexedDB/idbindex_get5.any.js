// META: global=window,worker
// META: title=IDBIndex.get() - throw DataError when using invalid key
// META: script=resources/support.js
// @author Intel <http://www.intel.com>

'use_strict';

let db;
const t = async_test();

const open_rq = createdb(t);
open_rq.onupgradeneeded = function(e) {
    db = e.target.result;

    const index = db.createObjectStore("test", { keyPath: "key" })
                    .createIndex("index", "indexedProperty");
    assert_throws_dom("DataError",function(){
        index.get(NaN);
    });
    t.done();
};
