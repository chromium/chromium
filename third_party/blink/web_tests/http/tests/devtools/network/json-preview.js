// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Network from 'devtools/panels/network/network.js';
import * as SourceFrame from 'devtools/ui/legacy/components/source_frame/source_frame.js';
import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that resources with JSON MIME types are previewed with the JSON viewer.\n`);
  await TestRunner.showPanel('network');

  function createNetworkRequestWithJSONMIMEType(type) {
    TestRunner.addResult('Creating a NetworkRequest with type: ' + type);
    var request = SDK.NetworkRequest.NetworkRequest.create(0, 'http://localhost');
    request.mimeType = type;
    request.setContentDataProvider(
        () => Promise.resolve(
            {error: null, content: '{"number": 42}', encoded: false}));
    return request;
  }

  async function testPreviewer(request) {
    var previewView = new Network.RequestPreviewView.RequestPreviewView(request, null);
    previewView.wasShown();
    var previewer = await previewView.contentViewPromise;
    TestRunner.addResult('Its previewer is searchable: ' + (previewer && previewer instanceof UIModule.SearchableView.SearchableView));
    TestRunner.addResult(
        'Its previewer is the JSON previewer: ' +
        (previewer && previewer.searchProvider && previewer.searchProvider instanceof SourceFrame.JSONView.JSONView));
  }

  function testType(contentType, callback) {
    var request = createNetworkRequestWithJSONMIMEType(contentType);
    testPreviewer(request).then(callback);
  }
  TestRunner.runTestSuite([
    function test1(next) {
      testType('application/json', next);
    },
    function test2(next) {
      testType('application/vnd.document+json', next);
    },
  ]);
})();
