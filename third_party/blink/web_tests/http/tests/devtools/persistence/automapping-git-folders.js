// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(
      `Verify that automapping is able to map ambiguous resources based on the selected project folder.\n`);

  var automappingTest = new BindingsTestRunner.AutomappingTest(new Workspace.Workspace.WorkspaceImpl());

  var reset_css = {content: '* { margin: 0 }', time: new Date('April 29, 1959')};
  var jquery_js = {content: 'window.superb = 1;', time: new Date('August 26, 2006')};
  var logo1 = {content: 'AAAA', time: new Date('June 12, 2012')};
  var logo2 = {content: 'BBBBBBBB', time: new Date('April 21, 2007')};

  automappingTest.addNetworkResources({
    'http://example.com/reset.css': reset_css,
    'http://example.com/jquery.js': jquery_js,
    'http://example.com/logo.png': logo2
  });

  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  BindingsTestRunner.addFiles(fs, {
    'code/proj1/.git/HEAD': {content: 'ref: refs/heads/master', time: new Date('May 12, 2007')},
    'code/proj1/reset.css': reset_css,
    'code/proj1/jquery.js': jquery_js,
    'code/proj1/logo.png': logo1,

    'code/proj2/.git/HEAD': {content: 'ref: refs/heads/master', time: new Date('May 25, 2009')},
    'code/proj2/reset.css': reset_css,
    'code/proj2/jquery.js': jquery_js,
    'code/proj2/logo.png': logo2,
  });
  fs.reportCreated(onFileSystemCreated);

  function onFileSystemCreated() {
    automappingTest.waitUntilMappingIsStabilized().then(TestRunner.completeTest.bind(TestRunner));
  }
})();
