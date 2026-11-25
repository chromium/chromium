<?php
http_response_code(307);
header('Access-Control-Allow-Origin: *');
header('x-devtools-redirect: 2');
header('location: http://redirect-three.test:8000/inspector-protocol/network/resources/redirect-3.php');
?>
