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
    import {mojo} from "/gen/mojo/public/js/bindings.js";
    import {NonAssociatedWebTestControlHostRemote} from
      "/gen/content/web_test/common/web_test.mojom.m.js";
    import {ByteString} from "/gen/mojo/public/mojom/base/byte_string.mojom.m.js";
    import {LCPCriticalPathPredictorNavigationTimeHint} from
      "/gen/third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.m.js";

    if (!window.testRunner) {
      console.log("This test requires window.testRunner.")
    }

    testRunner.dumpAsText();
    testRunner.waitUntilDone();

    if (window.location.search != "?start") {
      const hint = new LCPCriticalPathPredictorNavigationTimeHint();

      var getLCPBytes = async function(proto_file) {
        const resp = await fetch("/gen/third_party/blink/renderer/core/lcp_critical_path_predictor/test_proto/" + proto_file);
        const bytes = new ByteString;
        bytes.data = new Uint8Array(await resp.arrayBuffer());
        return bytes;
      };

      // All fields are non-nullable.
      hint.lcpElementLocators = [
        await getLCPBytes("lcp_image_id_b.pb")
      ];
      hint.lcpInfluencerScripts = [];
      hint.fetchedFonts = [];

      const web_test_control_host_remote = new NonAssociatedWebTestControlHostRemote();
      web_test_control_host_remote.$.bindNewPipeAndPassReceiver().bindInBrowser('process');
      web_test_control_host_remote.setLCPPNavigationHint(hint);

      window.location.search = '?start';
    }
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
