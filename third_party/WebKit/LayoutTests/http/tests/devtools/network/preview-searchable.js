// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that resources with JSON MIME types are previewed with the JSON viewer.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  async function testSearches(view, searches) {
    await new Promise(resolve => setTimeout(resolve, 0));
    for (var search of searches) {
      view._searchInputElement.value = search;
      view._regexButton.setToggled(false);
      view._caseSensitiveButton.setToggled(false);
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
    var isSearchable = (view instanceof UI.SearchableView);
    var compontentView = view;
    var typeName = 'unknown';
    var searchableView = view;
    if (view instanceof SourceFrame.ResourceSourceFrame.SearchableContainer) {
      isSearchable = true;
      searchableView = view.children()[0];
    }
    if (isSearchable)
      compontentView = searchableView._searchProvider;

    if (compontentView instanceof SourceFrame.ResourceSourceFrame) {
      typeName = 'ResourceSourceFrame';
      compontentView._ensureContentLoaded();
      if (!compontentView.loaded) {
        // try again when content is loaded.
        TestRunner.addSniffer(
            compontentView, 'setContent', previewViewHandled.bind(this, searches, callback, view));
        return;
      }
    } else if (compontentView instanceof SourceFrame.XMLView) {
      typeName = 'XMLView';
    } else if (compontentView instanceof SourceFrame.JSONView) {
      typeName = 'JSONView';
    } else if (compontentView instanceof Network.RequestHTMLView) {
      typeName = 'RequestHTMLView';
    } else if (compontentView instanceof UI.EmptyWidget) {
      typeName = 'EmptyWidget';
    } else if (compontentView instanceof Network.RequestHTMLView) {
      typeName = 'RequestHTMLView';
    }

    TestRunner.addResult('Is Searchable: ' + isSearchable);
    TestRunner.addResult('Type: ' + typeName);

    if (isSearchable)
      await testSearches(searchableView, searches);

    callback();
  }


  function trySearches(request, searches, callback) {
    TestRunner.addSniffer(Network.RequestPreviewView.prototype, '_doShowPreview', async function() {
      previewViewHandled(searches, callback, await this._contentViewPromise);
    });
    var networkPanel = UI.panels.network;
    networkPanel._showRequest(request);
    var itemView = networkPanel._networkItemView;
    itemView._selectTab('preview');
  }

  function testType(contentType, content, searches, callback) {
    var url = 'data:' + contentType + ',' + encodeURIComponent(content);
    NetworkTestRunner.makeSimpleXHR('GET', url, true, function() {
      var request = NetworkTestRunner.findRequestsByURLPattern(new RegExp(url.escapeForRegExp()))[0];
      request._resourceType = Common.resourceTypes.Document;
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
      testType('application/vnd.document+json', '{foo0foo: 123}', ['foo'], next);
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
      testType('text/xml', '{foo0: \'barr\', \'barr\': \'fooo\'}', ['fooo', 'bar'], next);
    }
  ]);
})();
