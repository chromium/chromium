// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SDKTestRunner} from 'sdk_test_runner';

import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Tests how switch to next file with the same name and different extension feature works.\n`);
  await TestRunner.showPanel('sources');

  var files = [
    'foo.css',     'foo.js',     'foo.js.map',     'foo.ts',

    'bar.css',     'bar.js',     'bar.js.map',     'bar.ts',

    'baz.css',     'baz2',

    'foo/foo.css', 'foo/foo.js', 'foo/foo.js.map', 'foo/foo.ts', 'foo/foo',

    'bar/foo.css', 'bar/foo.js', 'bar/foo.js.map', 'bar/foo.ts', 'bar/foo'
  ];
  files = files.map(file => 'http://example.com/' + file);

  var page = new SDKTestRunner.PageMock('http://example.com');
  page.connectAsMainTarget('mock-page');

  var uiSourceCodes = [];
  for (var i = 0; i < files.length; ++i) {
    page.evalScript(files[i], '', false /* isContentScript */);
    uiSourceCodes.push(await TestRunner.waitForUISourceCode(files[i]));
  }

  TestRunner.addResult('Dumping next file for each file:');
  for (var uiSourceCode of uiSourceCodes) {
    var nextUISourceCode = SourcesModule.SourcesView.SwitchFileActionDelegate.nextFile(uiSourceCode);
    var nextURI = nextUISourceCode ? nextUISourceCode.url() : '<none>';
    TestRunner.addResult(`Next file for ${uiSourceCode.url()} is ${nextURI}.`);
  }
  TestRunner.completeTest();
})();
