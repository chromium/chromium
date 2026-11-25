<?php
http_response_code(307);
header('Access-Control-Allow-Origin: *');
header('x-devtools-redirect: 1');
header('location: http://redirect-two.test:8000/inspector-protocol/network/resources/redirect-2.php');
?>
