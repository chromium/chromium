// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as BindingsModule from 'devtools/models/bindings/bindings.js';
import * as TextUtils from 'devtools/models/text_utils/text_utils.js';

(async function() {
  TestRunner.addResult(`Tests static content provider search.\n`);
  await TestRunner.showPanel('sources');

  await TestRunner.addIframe('resources/search.html');

  ApplicationTestRunner.runAfterResourcesAreFinished(['search.js'], step2);
  var resource;
  var staticContentProvider;

  function step2() {
    resource = BindingsModule.ResourceUtils.resourceForURL('http://127.0.0.1:8000/devtools/search/resources/search.js');
    resource.requestContent().then(step3);
  }

  async function step3() {
    staticContentProvider = TextUtils.StaticContentProvider.StaticContentProvider.fromString('', Common.ResourceType.resourceTypes.Script, resource.content);
    TestRunner.addResult(resource.url);

    var text = 'searchTestUniqueString';
    var searchMatches = await staticContentProvider.searchInContent(text, true, false);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    var text = 'searchTestUniqueString';
    searchMatches = await staticContentProvider.searchInContent(text, true, false);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    var text = '[a-z]earchTestUniqueString';
    searchMatches = await staticContentProvider.searchInContent(text, false, true);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    var text = '[a-z]earchTestUniqueString';
    searchMatches = await staticContentProvider.searchInContent(text, true, true);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    TestRunner.completeTest();
  }
})();
