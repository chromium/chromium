// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Platform from 'devtools/core/platform/platform.js';
import * as Network from 'devtools/panels/network/network.js';
import * as SourceFrame from 'devtools/ui/legacy/components/source_frame/source_frame.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Tests that resources with JSON MIME types are previewed with the JSON viewer.\n`);
  await TestRunner.showPanel('network');

  async function testSearches(view, searches) {
    await new Promise(resolve => setTimeout(resolve, 0));
    for (var search of searches) {
      view.searchInputElement.value = search;
      view.regexButton.toggled = false;
      view.caseSensitiveButton.toggled = false;
      view.showSearchField();
      TestRunner.addResult('Should have found and highlighted all: ' + search);

      var foundItems = view.element.childTextNodes()
                           .filter(node => node.parentElement.classList.contains('highlighted-search-result'))
                           .map(node => node.parentElement);
      TestRunner.addResult('Normal search found ' + foundItems.length + ' results in dom.');

      foundItems = view.element.childTextNodes()
                       .filter(node => node.parentElement.classList.contains('cm-search-highlight-start'))
                       .map(node => node.parentElement);
      TestRunner.addResult('CodeMirror search found ' + foundItems.length + ' results in dom.');
      TestRunner.addResult('');
    }
  }

  async function previewViewHandled(searches, callback, view) {
    var isSearchable = (view instanceof UI.SearchableView.SearchableView);
    var compontentView = view;
    var typeName = 'unknown';
    var searchableView = view;
    if (view instanceof SourceFrame.ResourceSourceFrame.SearchableContainer) {
      isSearchable = true;
      searchableView = view.children()[0];
    }
    if (isSearchable)
      compontentView = searchableView.searchProvider;

    if (compontentView instanceof SourceFrame.ResourceSourceFrame.ResourceSourceFrame) {
      typeName = 'ResourceSourceFrame';
      compontentView.ensureContentLoaded();
      if (!compontentView.loaded) {
        // try again when content is loaded.
        TestRunner.addSniffer(
            compontentView, 'setContent', previewViewHandled.bind(this, searches, callback, view));
        return;
      }
    } else if (compontentView instanceof SourceFrame.XMLView.XMLView) {
      typeName = 'XMLView';
    } else if (compontentView instanceof SourceFrame.JSONView.JSONView) {
      typeName = 'JSONView';
    } else if (compontentView instanceof Network.RequestHTMLView.RequestHTMLView) {
      typeName = 'RequestHTMLView';
    } else if (compontentView instanceof UI.EmptyWidget.EmptyWidget) {
      typeName = 'EmptyWidget';
    } else if (compontentView instanceof Network.RequestHTMLView.RequestHTMLView) {
      typeName = 'RequestHTMLView';
    }

    TestRunner.addResult('Is Searchable: ' + isSearchable);
    TestRunner.addResult('Type: ' + typeName);

    if (isSearchable)
      await testSearches(searchableView, searches);

    callback();
  }


  function trySearches(request, searches, callback) {
    var networkPanel = Network.NetworkPanel.NetworkPanel.instance();
    TestRunner.addSniffer(Network.RequestPreviewView.RequestPreviewView.prototype, 'doShowPreview', async function() {
      previewViewHandled(searches, callback, await this.contentViewPromise);
      networkPanel.hideRequestPanel();
    });
    networkPanel.onRequestSelected({data: request});
    networkPanel.showRequestPanel();
    var itemView = networkPanel.networkItemView;
    itemView.selectTab('preview');
  }

  function testType(contentType, content, searches, callback) {
    var url = 'data:' + contentType + ',' + encodeURIComponent(content);
    NetworkTestRunner.makeSimpleXHR('GET', url, true, function() {
      var request = NetworkTestRunner.findRequestsByURLPattern(new RegExp(Platform.StringUtilities.escapeForRegExp(url)))[0];
      request.setResourceType(Common.ResourceType.resourceTypes.Document);
      trySearches(request, searches, callback);
    });
  }

  TestRunner.runTestSuite([
    function plainTextTest(next) {
      testType('text/plain', 'foo bar\nfoo bar', ['foo', /*"bar"*/], next);
    },
    function jsonTest(next) {
      testType('application/json', '[533,3223]', ['533', '322'], next);
    },
    function jsonSpecialMimeTest(next) {
      testType('application/vnd.document+json', '{"foo0foo": 123}', ['foo'], next);
    },
    function xmlMultipleSearchTest(next) {
      testType('text/xml', '<bar><foo/>test</bar>', ['bar', 'foo', 'bar', 'test'], next);
    },
    function xmlSingleSearchTest(next) {
      testType('text/xml', '<bar></bar>', ['bar'], next);
    },
    function xmlCommentSearchTest(next) {
      testType('text/xml', '<bar><!-- TEST --></bar>', ['TEST', '/bar', 'bar'], next);
    },
    function xmlCDATASearchTest(next) {
      testType('text/xml', '<a><![CDATA[GGG]]><g tee="gee">tee</g></a>', ['GGG', 'tee', 'CDATA'], next);
    },
    function xmlMimeTypeJsonTest(next) {
      testType('text/xml', '{"foo0": "barr", "barr": "fooo"}', ['fooo', 'bar'], next);
    }
  ]);
})();
