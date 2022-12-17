<?php
header("Origin-Agent-Cluster: ?0");
?>
<!DOCTYPE html>
<html>
<head>
  <script src="../../../resources/testharness.js"></script>
  <script src="../../../resources/testharnessreport.js"></script>
</head>
<body >
  <script>
    let t = async_test("Test input color popup's keyboard usability when inside cross-process iframe.");

    let iframe = document.createElement("iframe");
    iframe.src = "http://localhost:8000/forms/resources/color-picker-keyboard-cross-domain-iframe.php";

    const runTest = t.step_func((event) => {
      // The eventSender.keyDown() invocations in the iframe create extra window messages.
      // Ignore those.
      if (typeof(event.data) !== "string" || !event.data.includes("Color result:")) {
        return;
      }

      // iframe should be cross-origin (and cross-process)
      assert_equals(iframe.contentDocument, null);

      assert_equals(event.data, "Color result: 50");
      t.done();
    }, "Check rValueContainer in cross-domain iframe.");

    window.onmessage = runTest;

    document.body.appendChild(iframe);
  </script>
</body>
</html>
