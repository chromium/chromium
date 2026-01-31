// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function truncateFailByQuota(fs) {
  fs.root.getFile('fd', {create: false, exclusive: false}, function(fileEntry) {
    fileEntry.createWriter(function(fileWriter) {
      var failedInTruncate = false;
      fileWriter.onerror = function(e) {
        failedInTruncate = true;
      };
      fileWriter.onwriteend = function(e) {
        if (failedInTruncate) {
          fail(e.currentTarget.error);
        } else {
          done();
        }
      };
      fileWriter.truncate(2500 * 1024);
    }, unexpectedErrorCallback)
  }, function(e) { fail('Open for 2nd truncate:' + fileErrorToString(e)); } );
}

function requestFileSystemSuccess(fs) {
  fs.root.getFile('fd', {create: true, exclusive: false}, function(fileEntry) {
    fileEntry.createWriter(function(fileWriter) {
      var failedInTruncate = false;
      fileWriter.onerror = function(e) {
        debug(e.currentTarget.error);
        failedInTruncate = true;
      };
      fileWriter.onwriteend = function() {
        if (failedInTruncate) {
          truncateFailByQuota(fs);
        } else {
          fail('Unexpectedly succeeded to truncate.  It should fail by quota.');
        }
      };
      fileWriter.truncate(10000 * 1024);
    }, unexpectedErrorCallback)
  }, function(e) { fail('Open for 1st truncate:' + fileErrorToString(e)); } );
}

function quotaSuccess(result, expectedQuota) {
  if (result.usage != 0)
    fail('Usage is not zero: ' + result.usage);
  if (result.quota != expectedQuota)
    fail('Estimated quota is not ' + expectedQuota + ': ' + result.quota);

  window.webkitRequestFileSystem(
      window.TEMPORARY,
      1024 * 1024,
      requestFileSystemSuccess,
      unexpectedErrorCallback);
}

function test() {
  const params = new URLSearchParams(window.location.search);
  const expectedQuota = Number(params.get('quota'));
  if (isNaN(expectedQuota) || expectedQuota < 0) {
    fail(
        'Missing or invalid "quota" URL parameter, url was ' +
        window.location.href);
    return;
  }

  if (navigator.storage) {
    debug('Querying usage and quota.');
    navigator.storage.estimate()
        .then(result => quotaSuccess(result, expectedQuota))
        .catch(unexpectedErrorCallback);
  } else {
    debug('This test requires navigator.storage.');
  }
}
