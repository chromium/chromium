// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test() {
  indexedDBTest(prepareDatabase, testValidKeys);
}

function prepareDatabase()
{
  db = event.target.result;
  db.createObjectStore('store');
}

var valid_keys = [
  "-Infinity",
  "-Number.MAX_VALUE",
  "-1",
  "-Number.MIN_VALUE",
  "0",
  "Number.MIN_VALUE",
  "1",
  "Number.MAX_VALUE",
  "Infinity",

  "new Date(0)",
  "new Date(1000)",
  "new Date(1317399931023)",

  "''",

  "'\x00'",
  "'a'",
  "'aa'",
  "'b'",
  "'ba'",

  "'\xA2'", // U+00A2 CENT SIGN
  "'\u6C34'", // U+6C34 CJK UNIFIED IDEOGRAPH (water)
  "'\uD834\uDD1E'", // U+1D11E MUSICAL SYMBOL G-CLEF (UTF-16 surrogate pair)
  "'\uFFFD'", // U+FFFD REPLACEMENT CHARACTER

  "[]",

  "[-Infinity]",
  "[-Number.MAX_VALUE]",
  "[-1]",
  "[-Number.MIN_VALUE]",
  "[0]",
  "[Number.MIN_VALUE]",
  "[1]",
  "[Number.MAX_VALUE]",
  "[Infinity]",

  "[new Date(0)]",
  "[new Date(1000)]",
  "[new Date(1317399931023)]",

  "['']",
  "['\x00']",
  "['a']",
  "['aa']",
  "['b']",
  "['ba']",

  "['\xA2']", // U+00A2 CENT SIGN
  "['\u6C34']", // U+6C34 CJK UNIFIED IDEOGRAPH (water)
  "['\uD834\uDD1E']", // U+1D11E MUSICAL SYMBOL G-CLEF (UTF-16 surrogate pair)
  "['\uFFFD']", // U+FFFD REPLACEMENT CHARACTER

  "[[]]",

  "[[], []]",
  "[[], [], []]",

  "[[[]]]",
  "[[[[]]]]"
];


var invalid_keys = [
  "void 0", // undefined
  "true",
  "false",
  "NaN",
  "null",
  "{}",
  "function () {}",
  "/regex/",
  "window",
  "window.document",
  "window.document.body",
  "(function() { var cyclic = []; cyclic.push(cyclic); return cyclic; }())"
];


function testValidKeys() {
  var test_keys = valid_keys.slice(); // make a copy
  var count = 0, when_complete = testInvalidKeys;
  testNextKey();

  function testNextKey() {
    var key = test_keys.shift();
    if (!key) {
      when_complete();
      return;
    }

    key = eval("(" + key + ")");
    var value = 'value' + (count++);
    var trans = db.transaction('store', 'readwrite');
    var store = trans.objectStore('store');
    var putreq = store.put(value, key);
    putreq.onerror = unexpectedErrorCallback;
    putreq.onsuccess = function() {
      getreq = store.get(key);
      getreq.onerror = unexpectedErrorCallback;
      getreq.onsuccess = function() {
        shouldBeEqualToString('getreq.result', value);
      };
    };
    trans.oncomplete = testNextKey;
  }
}

function testInvalidKeys() {

  var trans = db.transaction('store', 'readwrite');
  var store = trans.objectStore('store');

  invalid_keys.forEach(
    function(key) {
      try {
        key = eval("(" + key + ")");
        var putreq = store.put('value', key);
        putreq.onerror = unexpectedErrorCallback;
        putreq.onsuccess = unexpectedSuccessCallback;
        return;
      } catch (e) {
        window.ex = e;
        shouldBe("ex.code", "0");
        shouldBe("ex.name", "'DataError'");
      }
    });
  testKeyOrdering();
}

function testKeyOrdering() {

  for (var i = 0; i < valid_keys.length - 1; ++i) {
    var key1 = valid_keys[i];
    var key2 = valid_keys[i + 1];

    shouldBe("indexedDB.cmp(" + key1 + "," +  key2 + ")", "-1");
    shouldBe("indexedDB.cmp(" + key2 + "," +  key1 + ")", "1");
    shouldBe("indexedDB.cmp(" + key1 + "," +  key1 + ")", "0");
    shouldBe("indexedDB.cmp(" + key2 + "," +  key2 + ")", "0");
  }

  done();
}
