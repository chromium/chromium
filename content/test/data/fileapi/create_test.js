// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var filename = 'test.exe';

function readFile(fs)
{
  debug('Reading file.');
  fs.root.getFile(filename, {create:false},
    function(fileEntry) {
      fileEntry.file(
        function(blob) {
          var reader = new FileReader();
          reader.onloadend = function(e) {
            if (e.loaded == 4) {
              done();
            } else {
              fail('Incorrect read size: ' + e.loaded);
            }
          };
          reader.onerror = function(e) {
            fail('Read error:' + fileErrorToString(e));
          };
          reader.readAsArrayBuffer(blob);
        },
        function(e) { fail('read:' + fileErrorToString(e)); });
    },
    function(e) { fail('readFile getFile:' + fileErrorToString(e)); });
}

function writeFile(fs)
{
  debug('Creating file.');
  fs.root.getFile(filename, {create:true, exclusive:true},
    function(fileEntry) {
      fileEntry.createWriter(
        function(writer) {
          writer.onwriteend = function(e) {
            readFile(fs);
          };
          writer.onerror = function(e) {
            fail('Write:' + fileErrorToString(e));
          };
          var blob = new Blob([new Uint8Array([1, 2, 3, 4])],
                              {type:'application/octet-stream'});
          writer.write(blob);
        },
        function(e) { fail('createWriter:' + fileErrorToString(e)); });
    },
    function(e) { fail('writeFile getFile:' + fileErrorToString(e)); });
}

function requestFileSystemSuccess(fs)
{
  var fileDeleted = function() {
    writeFile(fs);
  };

  fs.root.getFile( filename, {create:false},
    function(fileEntry) {
      fileEntry.remove(
        fileDeleted,
        function(e) {
          fail('requestFileSystemSuccess remove:' + fileErrorToString(e));
        });
    },
    fileDeleted);
}

function test()
{
  debug('Requesting FileSystem');
  window.webkitRequestFileSystem(
      window.TEMPORARY,
      1024 * 1024,
      requestFileSystemSuccess,
      function(e) { fail('webkitRequestFileSystem:' + fileErrorToString(e)); });
}
