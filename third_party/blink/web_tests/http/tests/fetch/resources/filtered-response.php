<?php
// Headers filtered in 'basic filtered response'
header('Set-Cookie: cookie3=test-cookie');
header('Set-Cookie2: cookie4=test-cookie2');

$content = "Success.";

// Headers NOT filtered in 'CORS filtered response'
header('Cache-Control: private, no-store, no-cache, must-revalidate');
header('Content-Language: test-content-language');
header('Content-Length: ' . strlen($content));
header('Content-Type: test-content-type');
header('Expires: test-expires');
header('Last-Modified: test-last-modified');
header('Pragma: test-pragma');
if (isset($_GET['ACEHeaders']))
    header("Access-Control-Expose-Headers: {$_GET['ACEHeaders']}");

// Other headers
header('X-test: test-x-test');
header('X-test2: test-x-test2');

header('Access-Control-Allow-Origin: *');

echo $content;
?>
