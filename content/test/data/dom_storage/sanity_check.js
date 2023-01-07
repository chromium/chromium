// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function startTestSoon() {
  window.setTimeout(test, 0);
}

function test() {
  try {
    debug('Checking window.localStorage');
    sanityCheck(window.localStorage);
    debug('Checking window.sessionStorage');
    sanityCheck(window.sessionStorage);
    window.setTimeout(done, 0);
  } catch(e) {
    fail(e);
  }
}

function sanityCheck(storage) {
  storage["preload"] = "done";
  checkEqual("done", storage["preload"],
             "storage['preload'] != 'done' after addition");

  storage.clear();

  checkEqual(0, storage.length,
             "storage.length != 0 at start");
  checkEqual(null, storage.getItem("foo"),
             "getItem('foo') != null prior to addition");
  checkEqual(null, storage.key(0),
             "key(0) != null prior to addition");

  storage.setItem("foo", "bar");

  checkEqual(1, storage.length,
             "storage.length != 1 after addition");
  checkEqual("bar", storage.getItem("foo"),
             "getItem('foo') != 'bar' after addition");
  checkEqual("foo", storage.key(0),
             "key(0) != 'foo' after addition");

  storage.removeItem("foo");

  checkEqual(null, storage.getItem("foo"),
             "getItem('foo') != null after removal");

  storage["foo"] = "baz";
  storage["name"] = "value";

  checkEqual(2, storage.length,
             "storage.length != 2 after 2 additions");
  checkEqual("baz", storage["foo"],
             "storage['foo'] != 'baz' after addition");
  checkEqual("value", storage["name"],
             "storage['name'] != 'value' after addition");

  storage.clear();

  checkEqual(0, storage.length,
             "storage.length != 0 after clear");

  var tooLarge = "X".repeat((5 * 1024 * 1024) + 1);
  try {
    storage.setItem("tooLarge", tooLarge);
    throw "failed to throw exception for very large value";
  } catch(ex) {
    checkEqual(ex.code, 22,
               "ex.code != 22 for attempt to store a very large value");
  }
  try {
    storage.setItem(tooLarge, "key is too large");
    throw "failed to throw exception for very large key";
  } catch(ex) {
    checkEqual(ex.code, 22,
               "ex.code != 22 for attempt to store a very large key");
  }
}

function checkEqual(lhs, rhs, errorMessage) {
  if (lhs !== rhs)
    throw errorMessage;
}
