// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline UI API for network requests.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var image = new Image();
          var imagePromise = new Promise((fulfill) => image.onload = fulfill);
          image.src = "../resources/anImage.png";

          var script = document.createElement("script");
          script.src = "../resources/timeline-network-resource.js";
          document.body.appendChild(script);
          var scriptPromise = new Promise((fulfill) => window.timelineNetworkResourceEvaluated = fulfill);

          return Promise.all([imagePromise, scriptPromise]);
      }
  `);

  await TestRunner.NetworkAgent.setCacheDisabled(true);
  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  var model = PerformanceTestRunner.timelineModel();
  var linkifier = new Components.Linkifier();

  for (var request of model.networkRequests()) {
    var element = await Timeline.TimelineUIUtils.buildNetworkRequestDetails(request, model, linkifier);
    printElement(element);
  }
  TestRunner.completeTest();

  function printElement(element) {
    var rows = element.querySelectorAll('.timeline-details-view-row');
    for (var i = 0; i < rows.length; ++i) {
      var title = TestRunner.deepTextContent(rows[i].firstChild);
      var value = TestRunner.deepTextContent(rows[i].lastChild);
      if (title === 'Duration' || title === 'Mime Type' || title === 'Encoded Data')
        value = typeof value;
      if (/^file:\/\//.test(value))
        value = /[^/]*$/.exec(value)[0];
      if (!title && !value)
        continue;
      TestRunner.addResult(title + ': ' + value);
    }
  }
})();
