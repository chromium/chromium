// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Verify that automapping is sane.\n`);

  var timestamp = new Date('December 1, 1989');
  var index_html = {
    contentType: Common.ResourceType.resourceTypes.Document,
    content: '<body>this is main resource</body>',
    time: timestamp
  };
  var foo_js = {content: 'console.log(\'foo.js!\');', time: null};
  var bar_css = {
    contentType: Common.ResourceType.resourceTypes.Stylesheet,
    content: '* { box-sizing: border-box }',
    time: timestamp
  };
  var elements_module_json = {content: 'module descriptor 1'};
  var sources_module_json = {content: 'module descriptor 2'};
  var bazContent = 'alert(1);';

  var automappingTest = new BindingsTestRunner.AutomappingTest(new Workspace.Workspace.WorkspaceImpl());
  automappingTest.addNetworkResources({
    // Make sure main resource gets mapped.
    'http://example.com': index_html,

    // Make sure simple resource gets mapped.
    'http://example.com/path/foo.js': foo_js,

    // Make sure cache busting does not prevent mapping.
    'http://example.com/bar.css?12341234': bar_css,

    // Make sure files with different timestamps DO NOT map.
    'http://example.com/baz.js': {content: bazContent, time: new Date('December 3, 1989')},

    // Make sure files with different content lengths DO NOT map.
    'http://example.com/images/image.png': {content: '012345', time: timestamp},

    // Make sure assets are mapped based on path.
    'http://example.com/elements/module.json': elements_module_json,
    'http://example.com/sources/module.json': sources_module_json,
  });

  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  BindingsTestRunner.addFiles(fs, {
    'index.html': index_html,
    'scripts/foo.js': foo_js,
    'styles/bar.css': bar_css,
    'scripts/baz.js': {content: bazContent, time: new Date('December 4, 1989')},
    'images/image.png': {content: '0123456789', time: timestamp},
    'modules/elements/module.json': elements_module_json,
    'modules/sources/module.json': sources_module_json
  });
  fs.reportCreated(onFileSystemCreated);

  function onFileSystemCreated() {
    automappingTest.waitUntilMappingIsStabilized().then(TestRunner.completeTest.bind(TestRunner));
  }
})();
