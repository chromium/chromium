// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests name() and path() methods of NetworkRequest.\n`);
  await TestRunner.showPanel('network');

  /**
     * @param {string} url
     * @param {string=} targetUrl
     */
  function createNetworkRequestForURLAndDumpNameAndPath(url, targetUrl) {
    var mainTarget = SDK.targetManager.mainTarget();
    var currentTargetURL = mainTarget.inspectedURL();
    var dispatcher = TestRunner.networkManager._dispatcher;
    if (targetUrl)
      mainTarget.setInspectedURL(targetUrl);
    TestRunner.addResult('Dumping request name and path for url: ' + url);
    var request = dispatcher._createNetworkRequest(0, '', '', url);
    TestRunner.addResult('    name = ' + request.name());
    TestRunner.addResult('    path = ' + request.path());
    TestRunner.addResult('    targetUrl = ' + (targetUrl ? targetUrl : currentTargetURL));
    if (targetUrl !== currentTargetURL)
      mainTarget.setInspectedURL(currentTargetURL);
  }

  // Save the target URL to ensure test works well with other tests.
  var mainTarget = SDK.targetManager.mainTarget();
  var originalTargetURL = mainTarget.inspectedURL();
  mainTarget.setInspectedURL('http://127.0.0.1/aFolder/aTest.html');

  createNetworkRequestForURLAndDumpNameAndPath('http://www.example.com/foo/bar/baz?key=value');
  createNetworkRequestForURLAndDumpNameAndPath('http://www.example.com/foo/bar/?key=value');
  createNetworkRequestForURLAndDumpNameAndPath('http://www.example.com/foo/bar/baz');
  createNetworkRequestForURLAndDumpNameAndPath('http://www.example.com/foo/bar/');

  createNetworkRequestForURLAndDumpNameAndPath('http://www.example.com/baz?key=value');
  createNetworkRequestForURLAndDumpNameAndPath('http://www.example.com/?key=value');
  createNetworkRequestForURLAndDumpNameAndPath('http://www.example.com/baz');
  createNetworkRequestForURLAndDumpNameAndPath('http://www.example.com/');

  createNetworkRequestForURLAndDumpNameAndPath('http://127.0.0.1/foo/bar/baz?key=value');
  createNetworkRequestForURLAndDumpNameAndPath('http://127.0.0.1/foo/bar/?key=value');
  createNetworkRequestForURLAndDumpNameAndPath('http://127.0.0.1/foo/bar/baz');
  createNetworkRequestForURLAndDumpNameAndPath('http://127.0.0.1/foo/bar/');

  createNetworkRequestForURLAndDumpNameAndPath('http://127.0.0.1/baz?key=value');
  createNetworkRequestForURLAndDumpNameAndPath('http://127.0.0.1/?key=value');
  createNetworkRequestForURLAndDumpNameAndPath('http://127.0.0.1/baz');
  createNetworkRequestForURLAndDumpNameAndPath('http://127.0.0.1/');
  createNetworkRequestForURLAndDumpNameAndPath(
      'http://127.0.0.1/', 'data:text/html,<html><body><img src="data:,foobar" /></body></html>');
  createNetworkRequestForURLAndDumpNameAndPath(
      'data:text/html,<html><body><img src="data:,foobar" /></body></html>',
      'data:text/html,<html><body><img src="data:,foobar" /></body></html>');
  createNetworkRequestForURLAndDumpNameAndPath(
      'data:text/html,<html><body><img src="data:,foobar" /></body></html>', 'http://127.0.0.1/');

  // Restore the target URL to ensure test works well with other tests.
  mainTarget.setInspectedURL(originalTargetURL);

  TestRunner.completeTest();
})();
