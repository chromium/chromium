<?php
header("Document-Policy: oversized-images=2.0");
?>
<!DOCTYPE html>

<head>
  <script src='/resources/testharness.js'></script>
  <script src='/resources/run-after-layout-and-paint.js'></script>
</head>

<body>
  <!--
    Tests that document policy violation on oversized-images is not triggered,
    after images are resized to proper size.

    The initial sizes setting for images all trigger document policy violation.
    It is expected that 4 violation report and corresponding console messages
    generated. (content in -expected.txt)

    After the resize, there should not be any violations triggered.
    It is expected that non of images on page are replaced by placeholders
    after resize. (content in -expected.png)
  -->

  <!-- Note: give each image a unique URL so that the violation report is
    actually sent, instead of being ignored as a duplicate. -->

  <img src="resources/green-256x256.jpg?id=1" width="100" height="128">
  <img src="resources/green-256x256.jpg?id=2" style="height: 100px; width: 128px">
  <img src="resources/green-256x256.jpg?id=3" width="100" height="100">
  <img src="resources/green-256x256.jpg?id=4" style="height: 100px; width: 100px">

  <script type="module">
    if (window.testRunner) {
      window.testRunner.waitUntilDone();
    }

    const reports = new Promise(resolve => {
      let num_reports = 0;
      const observer = new ReportingObserver((reports, observer) => {
        for (const report of reports) {
          if (report.body.featureId != "oversized-images") {
            continue;
          }
          num_reports++;
          assert_less_than_equal(num_reports, 4);
          if (num_reports == 4) {
            resolve();
          }
        }
      })
      observer.observe();
    });

    const images = document.getElementsByTagName('img');

    // Wait for every image to load:
    for(const image of images) {
      if (!image.complete) {
        await new Promise(r => image.onload = r);
      }
    }

    // Wait for them to be painted on screen a first time:
    await new Promise(r => runAfterLayoutAndPaint(r));

    // Resize them:
    for(const image of images) {
      if (image.hasAttribute('width') || image.hasAttribute('height')) {
        image.width = "150";
        image.height = "150";
      } else {
        image.style.width = "150px";
        image.style.height = "150px";
      }
    }

    // Wait for them to be painted on screen again:
    await new Promise(r => runAfterLayoutAndPaint(r));

    // We are expecting exactly 4 reports to be sent:
    await reports;

    // Wait more to give the opportunity for an unexpected reports to be sent:
    // Note: This potentially help avoiding this test to be flaky. See
    // https://crbug.com/1367514.
    await new Promise(r => step_timeout(r, 500));

    if (window.testRunner) {
      window.testRunner.notifyDone();
    }
  </script>
</body>

</html>
