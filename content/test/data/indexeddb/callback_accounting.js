// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test()
{
  var dbbase = "callback_accounting_";

  var request1 = indexedDB.open(dbbase + 1);

  request1.onupgradeneeded = function() {
    debug("request1 open onupgradeneeded");
    request1.result.createObjectStore('store');
  };

  request1.onsuccess = function() {
    debug("request1 open onsuccess");

    var db1 = request1.result;
    var transaction = db1.transaction('store');
    transaction.onabort = unexpectedAbortCallback;

    debug("transaction created and looping");
    endTransaction = false;
    transactionRunning = true;
    function loop() {
      if (!endTransaction) {
        transaction.objectStore('store').get(0).onsuccess = loop;
      }
    }
    loop();

    var request2 = indexedDB.open(dbbase + 2);

    request2.onsuccess = function() {
      debug("request2 open onsuccess");

      shouldBeTrue("transactionRunning");
      var db2 = request2.result;
      db2.close();
      debug("db2 close2");
      endTransaction = true;
      debug("ending transaction");
    };

    transaction.oncomplete = function() {
      debug("transaction oncomplete");
      shouldBeTrue("endTransaction");
      done();
    };
  };
}
