// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`To make sure that filenames are encoded safely for Network Persistence.\n`);

  await TestRunner.loadTestModule('bindings_test_runner');

  var {project} = await BindingsTestRunner.createOverrideProject('file:///tmp/');
  BindingsTestRunner.setOverridesEnabled(true);

  // Simple tests.
  log('www.example.com/');
  log('www.example.com/simple');
  log('www.example.com/hello/foo/bar');
  log('www.example.com/.');

  // Reserved names on windows.
  log('example.com/CON');
  log('example.com/cOn');
  log('example.com/cOn/hello');
  log('example.com/PRN');
  log('example.com/AUX');
  log('example.com/NUL');
  log('example.com/COM1');
  log('example.com/COM2');
  log('example.com/COM3');
  log('example.com/COM4');
  log('example.com/COM5');
  log('example.com/COM6');
  log('example.com/COM7');
  log('example.com/COM8');
  log('example.com/COM9');
  log('example.com/LPT1');
  log('example.com/LPT2');
  log('example.com/LPT3');
  log('example.com/LPT4');
  log('example.com/LPT5');
  log('example.com/LPT6');
  log('example.com/LPT7');
  log('example.com/LPT8');
  log('example.com/LPT9');

  // Query params
  log('example.com/fo?o/bar');
  log('example.com/foo?/bar');
  log('example.com/foo/?bar');
  log('example.com/foo/?bar');
  log('example.com/?foo/bar/3');

  // Hash params
  log('example.com/?foo/bar/3#hello/bar');
  log('example.com/#foo/bar/3hello/bar');
  log('example.com/foo/bar/?3hello/bar');
  log('example.com/foo/bar/#?3hello/bar');
  log('example.com/foo.js#');

  // Windows cannot end in . (period).
  log('example.com/foo.js.');
  // Windows cannot end in (space).
  log('example.com/foo.js ');

  // Others
  log('example.com/foo .js');
  log('example.com///foo.js');
  log('example.com///');

  // Very long file names
  log('example.com' + '/THIS/PATH/IS_MORE_THAN/200/Chars'.repeat(8));
  log('example.com' + '/THIS/PATH/IS_LESS_THAN/200/Chars'.repeat(5));

  TestRunner.completeTest();

  function log(url) {
    TestRunner.addResult(url + ' -> ' + Persistence.networkPersistenceManager.encodedPathFromUrl(url));
  }
})();
