// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function requestFileSystemSuccess(fs)
{
  debug('Requested successfully.');
  done();
}

function test()
{
  debug('Requesting FileSystem');
  window.webkitRequestFileSystem(
      window.TEMPORARY,
      1024 * 1024,
      requestFileSystemSuccess,
      unexpectedErrorCallback);
}
