// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Network from 'devtools/panels/network/network.js';
import * as SourceFrame from 'devtools/ui/legacy/components/source_frame/source_frame.js';
import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  'use strict';
  TestRunner.addResult(`Tests to make sure the proper view is used for the data that is received in network panel.\n`);
  await TestRunner.showPanel('network');

  function createNetworkRequest(mimeType, content, statusCode, resourceType) {
    var request = SDK.NetworkRequest.NetworkRequest.create(0, 'http://localhost');
    request.setResourceType(resourceType);
    request.mimeType = mimeType;
    request.setContentDataProvider(() => Promise.resolve({error: null, content: content, encoded: false}));
    if (statusCode !== undefined)
      request.statusCode = statusCode;
    return request;
  }

  function getViewName(previewer) {
    if (!previewer)
      return '** NONE **';
    if (previewer instanceof SourceFrame.ResourceSourceFrame.SearchableContainer)
      return 'SearchableContainer > ' + getViewName(previewer.children()[0]);
    if (previewer instanceof UIModule.SearchableView.SearchableView)
      return 'SearchableView > ' + getViewName(previewer.searchProvider);
    return previewer.contentElement.className;
  }

  async function testPreviewer(mimeType, content, statusCode) {
    var testResourceTypes = [
      Common.ResourceType.resourceTypes.XHR, Common.ResourceType.resourceTypes.Fetch, Common.ResourceType.resourceTypes.Document, Common.ResourceType.resourceTypes.Other
    ];
    TestRunner.addResult('Testing with MimeType: ' + mimeType + ', and StatusCode: ' + statusCode);
    TestRunner.addResult('Content: ' + content.replace(/\0/g, '**NULL**'));
    TestRunner.addResult('');
    for (var resourceType of testResourceTypes) {
      var request = createNetworkRequest(mimeType, content, statusCode, resourceType);
      var previewView = new Network.RequestPreviewView.RequestPreviewView(request);
      previewView.wasShown();
      TestRunner.addResult(
          'ResourceType(' + resourceType.name() + '): ' + getViewName(await previewView.contentViewPromise));
    }
    TestRunner.addResult('');
  }

  TestRunner.addResult('Simple JSON');
  await testPreviewer('application/json', '[533,3223]', 200);

  TestRunner.addResult('MIME JSON');
  await testPreviewer('application/vnd.document+json', '{"foo0foo": 123}', 200);

  TestRunner.addResult('Simple XML');
  await testPreviewer('text/xml', '<bar><foo/></bar>', 200);

  TestRunner.addResult('XML MIME But JSON');
  await testPreviewer('text/xml', '{"foo0": "barr", "barr": "fooo"}', 200);

  TestRunner.addResult('HTML MIME But JSON');
  await testPreviewer('text/html', '{"hi": "hi"}', 200);

  TestRunner.addResult('TEXT MIME But JSON');
  await testPreviewer('text/html', '{"hi": "hi"}', 200);

  TestRunner.addResult('HTML MIME With 500 error');
  await testPreviewer('text/html', 'This\n<b>is a </b><br /><br />test.', 500);

  TestRunner.addResult('Plain Text MIME But HTML/XML');
  await testPreviewer('text/plain', 'This\n<b>is a </b><br /><br />test.', 200);

  TestRunner.addResult('XML With Unknown MIME');
  await testPreviewer('text/foobar', '<bar><foo/></bar>', 200);

  TestRunner.addResult('XML With Error 500');
  await testPreviewer('text/xml', '<bar><foo/></bar>', 500);

  TestRunner.addResult('Unknown MIME Text With Error 500');
  await testPreviewer('text/foobar', 'Foo Bar', 500);

  TestRunner.addResult('Binary Image File');
  await testPreviewer('image/png', 'Bin\0ary\x01 File\0\0', 200);

  TestRunner.addResult('Binary Blank Image File');
  await testPreviewer('image/png', '', 200);

  TestRunner.completeTest();
})();
