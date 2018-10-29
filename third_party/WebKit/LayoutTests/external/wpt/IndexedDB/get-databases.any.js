// META: script=support.js

async_test( async function(t) {
  let made_database_check = t.step_func(async function() {
    let idb_databases_promise = await indexedDB.databases();
    assert_true(
      idb_databases_promise.some(
          e => e.name == "TestDatabase" && e.version == 1),
      "Call to databases() did not find database.");
    t.done();
  });
  delete_then_open(t, "TestDatabase", ()=>{}, made_database_check);
}, "Report one database test.");

async_test( function(t) {
  let done_making_databases_callback = t.step_func(async function() {
    let idb_databases_promise = await indexedDB.databases();
    assert_true(
      idb_databases_promise.some(
          e => e.name == "TestDatabase1" && e.version == 1),
      "Call to databases() did not find database.");
    assert_true(
      idb_databases_promise.some(
          e => e.name == "TestDatabase2" && e.version == 1),
      "Call to databases() did not find database.");
    assert_true(
      idb_databases_promise.some(
          e => e.name == "TestDatabase3" && e.version == 1),
      "Call to databases() did not find database.");
    t.done();
  });
  let make_databases_barrier = create_barrier(done_making_databases_callback);
  delete_then_open(t, "TestDatabase1", ()=>{}, make_databases_barrier(t));
  delete_then_open(t, "TestDatabase2", ()=>{}, make_databases_barrier(t));
  delete_then_open(t, "TestDatabase3", ()=>{}, make_databases_barrier(t));
}, "Report multiple databases test.");

async_test( function(t) {
  let delete_request = indexedDB.deleteDatabase("NonExistentDatabase");
  delete_request.onsuccess = t.step_func(async function() {
    let idb_databases_promise = await indexedDB.databases();
    assert_false(
      idb_databases_promise.some(
          e => e.name == "NonExistentDatabase"),
      "Call to databases() found excluded database.");
    t.done();
  });
}, "Don't report nonexistant databases test.");

done();
