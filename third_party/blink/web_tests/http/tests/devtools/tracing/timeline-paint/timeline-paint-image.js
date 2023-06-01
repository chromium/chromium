// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a paint image event\n\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
    function addImage(url, width, height)
    {
        var img = document.createElement('img');
        img.style.position = "absolute";
        img.style.top = "100px";
        img.style.left = "0px";
        img.style.width = width;
        img.style.height = height;
        img.src = url;
        document.body.appendChild(img);
    }

    function addCSSImage(url, width, height)
    {
        var img = document.createElement('div');
        img.style.position = "absolute";
        img.style.top = "100px";
        img.style.left = "100px";
        img.style.width = width;
        img.style.height = height;
        img.style.background = \`url(\${JSON.stringify(url)})\`;
        document.body.appendChild(img);
    }

    function display()
    {
        // Note that the widths here are important: we sort events by the width
        // to ensure that the assertions are consistent and do not flake if the order
        // changes.
        addImage("../resources/test.png", "40px", "30px");
        addCSSImage("//:0", "30px", "20px"); // should be ignored, see https://crbug.com/776940
        addCSSImage("../resources/test.png", "20px", "20px");
        // Long URL (1338 characters)
        addCSSImage("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAADoElEQVR4nK2WzY4bRRSFv1tV/ed225SigSgiCx6AF+AJ2CTvAXv27Fmxy1vwAKyyZ8kqQooQUQQsBjwzjstjuy+Lqm6XPWPLIFpH1q3qdp+qW+ee206pqSvqOqGpH4//262mdn89V2/WVOxRn4jzYQEFOLBgSJdCD1vYwgYMCG7xDJx6t6bkANVlw0jj0uv2NDvYgsEtPlZKoVBfrdO6cpQXz4y7OaRxiydKpdRCpb4JaTkJmoIii0/Oa4otGEU00ribudIIjTIRGvVtwHIO7uK78QxuOpgoE6FVpkKrvgsY/gXsubvutoEWWqUTpspM6NTPA8L/AndXKRU0MIEpdMpcmKv3K9AMgIq8BlRfHs0/iEfZqnt3Rxv3gE6RKXToDOnQuQ/5asbrDwKDXuL8wxgQMOB+X2qNxA00aItMBrIW7XwY8/mFvIkE7wmXH5D7c0mFjkXaQA3NkLMGbf3KonbY+A/6+TvSzKM4Upm7XmqBFPsi1QqpiKxSQ4U2Pnwrt5HgV8JBqYz1cAp/f8ChFin2TyfKAi2REoph+V/r1VvC+bo+mne3qyhlNUi2NbWIA4s65KfPtpHgF8J5x3o445arJACDDoGMQ5N55fNr+4ZVheaoj4dH5qsuBM0lmPSeDfoXKXi/2AHi77ZyB3yqT2u2NbsMfU3fHA4d61IROaw/xUjSscA1YF9/wsKAUQz8BgSuLmkgjlDEN8Z8SHIWq8llbCTQm6vBzFwkWPFUKTNUmiU/DpXScW/Zv8sqVpIljkqLifr5KI9L+XEJT/Sr8+3CcS+HbuuUQpK44x/OXYFnj6l/XxiRYJDPAUdSnXzzJW3NtGJa09Uyq/sX3wPm7XfMm0B7tj9YxybaXn7I8QzGTVRQSbKPRpkkqS0+gqnSrnynmCMwBI5NDybz2JHDZCdRKKUkkTTpwcUMpjBTuuDnpxqCYxfbc4QeGjp56oa8VfLqFV3HTWogMFfmwftHD8mx2aaX74ZvjQ1sYA1rCLCCD7CEBo0eOzQQpgPFDO1Yzb0KewAGx3ZHvofxs+ke7iFAGCzmyNAnENvtJJFpS+j8cU+m72FITw8WduCGfThYZ/quBrITDUQbQutzHTm0pwcBBZPlysI2+xIp2MtqrJAy4xvtrSI0fiwJhyrapzOPmzBDrmympoNazCgfY9WCVeW1QAtc0oz2jPUQaQR2g92ZIcgpzYOqcgcrCIWnTFbzgIPsNw+OyjEP8qUMCwrW/wPZ2mq+jvKj/AAAAABJRU5ErkJggg==", "50px", "60px");

        return waitForFrame();
    }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('display');

  const events = PerformanceTestRunner.mainTrackEvents()
      .filter(e => e.name === TimelineModel.TimelineModel.RecordType.PaintImage);

  const imageDataUri = events.find(e => e.args.data.url.startsWith('data:')).args.data.url;
  const urlIsElided = imageDataUri.includes('...') && imageDataUri.length < 1338;
  TestRunner.assertTrue(urlIsElided, 'URL was not elided');

  TestRunner.assertEquals(events.length, 3, 'PaintImage records not found');
  // The events can arrive in different order on different test runs, so sort
  // by the width of the image to ensure the same order for assertions.
  events.sort((eventA, eventB) => {
    return eventA.args.data.width - eventB.args.data.width;
  })
  events.forEach(e => PerformanceTestRunner.printTraceEventProperties(e));

  TestRunner.completeTest();
})();
