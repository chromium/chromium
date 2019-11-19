// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the instrumentation of a DecodeImage and ResizeImage events\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
    <style>
    div {
        display: inline-block;
        border-style: solid;
    }

    div.img-container {
        position: relative;
        width: 99px;
        height: 99px;
        overflow: clip;
    }

    .background, .border {
        width: 25px;
        height: 25px;
    };

    .border {
        border-width: 12px;
    }

    </style>
  `);
  await TestRunner.addScriptTag('../../../resources/run-after-layout-and-paint.js');

  await TestRunner.evaluateInPagePromise(`
    var images = [
        ["./resources/test.webp", "25", "25"],
        ["./resources/test.bmp", "25", "25"],
        ["./resources/test.gif", "25", "25"],
        ["./resources/test.ico", "25", "25"],
        ["./resources/test.jpg", "25", "25"],
        ["./resources/test.png", "25", "25"],
        ["./resources/big.png", "150", "150"]
    ];

    async function showImages()
    {
        for (let image of images) {
            await addImage(image);
            await new Promise(fulfill => runAfterLayoutAndPaint(fulfill));
        }
        return generateFrames(3);

        function addImage(image)
        {
            var imgContainer = document.createElement("div");
            imgContainer.className = "img-container";
            document.body.appendChild(imgContainer);

            var imgElement = document.createElement("img");
            var promise = new Promise((fulfill) => imgElement.onload = fulfill);
            imgContainer.appendChild(imgElement);

            var backgroundElement = document.createElement("div");
            backgroundElement.className = "background";
            document.body.appendChild(backgroundElement);

            var borderElement = document.createElement("div");
            borderElement.className = "border";
            document.body.appendChild(borderElement);

            imgElement.width = image[1];
            imgElement.height = image[2];
            imgElement.src = image[0];
            backgroundElement.style.backgroundImage = "url(" + image[0] + "?background)";
            borderElement.style.borderImage = "url(" + image[0] + "?border)";

            return promise;
        }
    }
  `);

  PerformanceTestRunner.invokeWithTracing('showImages', TestRunner.safeWrap(onTracingComplete));
  function onTracingComplete() {
    function isDecodeImageEvent(event) {
      return event.name === TimelineModel.TimelineModel.RecordType.DecodeImage;
    }
    function compareImageURLs(a, b) {
      var urlA = TimelineModel.TimelineData.forEvent(a).url;
      var urlB = TimelineModel.TimelineData.forEvent(b).url;
      urlA = TestRunner.formatters.formatAsURL(urlA || '<missing>');
      urlB = TestRunner.formatters.formatAsURL(urlB || '<missing>');
      return urlA.localeCompare(urlB);
    }
    var events = PerformanceTestRunner.timelineModel().inspectedTargetEvents();
    var sortedDecodeEvents = events.filter(isDecodeImageEvent).sort(compareImageURLs);
    for (var i = 0; i < sortedDecodeEvents.length; ++i) {
      var event = sortedDecodeEvents[i];
      var timlelineData = TimelineModel.TimelineData.forEvent(event);
      var url = timlelineData.url;
      // Skip duplicate events, as long as they have the imageURL
      if (i && url && url === TimelineModel.TimelineData.forEvent(sortedDecodeEvents[i - 1]).url)
        continue;
      TestRunner.addResult('event: ' + event.name);
      TestRunner.addResult('imageURL: ' + TestRunner.formatters.formatAsURL(url));
      TestRunner.addResult('backendNodeId: ' + (timlelineData.backendNodeId > 0 ? 'present' : 'absent'));
    }
    TestRunner.completeTest();
  }
})();
