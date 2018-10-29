<?php
header("X-XSS-Protection: 1; mode=block");
?>
<!DOCTYPE html>
<html>
<head>
<script src="http://127.0.0.1:8000/security/xssAuditor/resources/utilities.js"></script>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpChildFrames();
    testRunner.waitUntilDone();
    testRunner.setXSSAuditorEnabled(true);
}
</script>
</head>
<body>
<p>This tests that the header X-XSS-Protection is not inherited by the iframe below:</p>
<iframe id="frame" name="frame" onload="checkIfFrameLocationMatchesSrcAndCallDone('frame')" src="http://127.0.0.1:8000/security/xssAuditor/resources/echo-intertag.pl?q=<script>alert(/XSS/)</script><p>If you see this message and no JavaScript alert() then the test PASSED.</p>">
</iframe>
</body>
</html>
