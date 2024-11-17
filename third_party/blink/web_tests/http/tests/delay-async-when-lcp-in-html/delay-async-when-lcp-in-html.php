<!doctype html>
<head>
  <title>delay-async-when-lcp-in-html executes async scripts after lcp</title>
  <script src="/priorities/resources/common.js"></script>
  <script src="/resources/testharness.js"></script>
  <script src="/resources/testharnessreport.js"></script>
  <script>
    window.result = [];
    function logScript(msg) {
      window.result.push(msg);
    }
  </script>
  <script type=module>
import {setupLCPTest} from "../lcp_critical_path_predictor/resources/common.js";
await setupLCPTest(["lcp_image_id_b.pb"]);
  </script>
  <?php
    // Do not output the HTML below this PHP block until the test is reloaded with
    // "?start" to avoid it being picked up by the HTMLPreloadScanner.
    if ($_SERVER['QUERY_STRING'] != "start") {
      exit;
    }
  ?>
</head>
<body>
  <script>
    setup({single_test: true});

    assert_implements(window.LargestContentfulPaint, "LargestContentfulPaint is not implemented");
    const observer = new PerformanceObserver(list => {
      const entries = list.getEntries();
      for (const entry of entries) {
        assert_equals(entry.entryType, 'largest-contentful-paint');
        logScript('lcp')
      }
    });
    observer.observe({type: 'largest-contentful-paint', buffered: true});

    function finish() {
      const firstLcpIndex = window.result.indexOf('lcp');
      assert_true(firstLcpIndex >= 0);
      for (let i = 0; i < window.result.length; i++) {
        if (window.result[i] == 'async.js') {
          assert_true(i > firstLcpIndex);
        }
      }
      done();
    }
    window.addEventListener('load', finish);
  </script>
  <script src="/delay-async-when-lcp-in-html/resources/async1.js" async></script>
  <img src="/resources/square.png" id="lcp_image_b">
  <img src="/resources/square100.png">
  <script src="/delay-async-when-lcp-in-html/resources/async2.js" async></script>
</body>
</html>
