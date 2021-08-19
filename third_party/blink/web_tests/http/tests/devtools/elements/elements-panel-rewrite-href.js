// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that src and href element targets are rewritten properly.\n`);
  await TestRunner.loadModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
      <a style="display:none" href=" javascript:alert('foo') "></a>
      <script>
        (function(){
          let iframe = document.createElement('iframe');
          iframe.src = "resources/elements-panel-rewrite-href-iframe.html";
          document.body.appendChild(iframe);
          window.frameLoadedPromise = new Promise(f => iframe.onload = f);
        })();
      </script>
    `);

  await TestRunner.evaluateInPageAsync(`window.frameLoadedPromise`);

  ElementsTestRunner.expandElementsTree(step1);

  function step1() {
    var innerMapping = TestRunner.domModel.idToDOMNode;

    var outputLines = [];
    for (var nodeId in innerMapping) {
      var node = innerMapping[nodeId];
      if (node.nodeName() === 'LINK' || node.nodeName() === 'SCRIPT') {
        var segments = [];
        var href = node.resolveURL(node.getAttribute('src') || node.getAttribute('href'));
        if (!href) {
          segments.push('<empty>');
          continue;
        }
        if (href.startsWith('http:')) {
          outputLines.push(href);
          continue;
        }
        var parsedURL = href.asParsedURL();
        if (!parsedURL) {
          testController.notifyDone('FAIL: no URL match for <' + href + '>');
          continue;
        }
        var split = parsedURL.path.split('/');
        for (var i = split.length - 1, j = 0; j < 3 && i >= 0; --i, ++j)
          segments.push(split[i]);
        outputLines.push(segments.reverse());
      }
      if (node.nodeName() === 'A')
        outputLines.push(node.resolveURL(node.getAttribute('href')));
    }
    outputLines.sort();
    TestRunner.addResult(outputLines.join('\n'));
    TestRunner.completeTest();
  }
})();
