<?php
require_once '../resources/portabilityLayer.php';

// This test loads an uncacheable main resource and a cacheable image subresource.
// We then trigger a POST request, which redirects as a GET back to this page.
// On this GET request, the image should be loaded from the cache and no HTTP
// request should be sent. The image resource will return 404 if it receives
// a second request, which will cause us to FAIL.
// See https://bugs.webkit.org/show_bug.cgi?id=38690
header('Cache-Control: no-cache, no-store, must-revalidate, max-age=0');
header('Pragma: no-cache');
header('Expires: ' . gmdate(DATE_RFC1123, time()-1));

$submit = $_POST["submit"] ?? null;
if ($submit == "redirect") {
    header('HTTP/1.1 302 Found');
    header('Location: post-redirect-get.php?finish=true');
    exit();
}
?>
<html>
<body>
<div id="result"></div>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}
    
function logAndFinish(message)
{
    document.getElementById("result").appendChild(document.createTextNode(message));
    var xhr = new XMLHttpRequest;
    xhr.open("GET", "../resources/reset-temp-file.php?filename=post-redirect-get.tmp", false);
    xhr.send(null);
    if (window.testRunner)
        testRunner.notifyDone();
}
</script>
<?php
clearstatcache();
if ($_GET["finish"] == "true") {
    // The initial load of the image might have been done from the cache. In that case, touch the
    // state file to indicate a load has already occurred.
    if (!file_exists(sys_get_temp_dir() . "/post-redirect-get.tmp")) {
        echo "<script>";
        echo "xhr = new XMLHttpRequest;";
        echo "xhr.open('GET', '../resources/touch-temp-file.php?filename=post-redirect-get.tmp', false);";
        echo "xhr.send(null);";
        echo "</script>";
    }
    echo "<img src='resources/post-image-to-verify.php?filename=post-redirect-get.tmp' onload=\"logAndFinish('PASS');\" onerror=\"logAndFinish('FAIL');\"></img>";
} else {
    echo "<form action='post-redirect-get.php' method='post'>";
    echo "<input type='submit' id='submit' name='submit' value='redirect'>";
    echo "</form>";
    echo "<img src='resources/post-image-to-verify.php?filename=post-redirect-get.tmp' onload=\"document.getElementById('submit').click();\" onerror=\"logAndFinish('FAIL on initial load');\"></img>";
} 
?>
</body>
</html>
