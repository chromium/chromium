<?php
http_response_code(307);
header('x-devtools-redirect: 2');
header('location: http://redirect-three.test:8000/devtools/network/resources/redirect-3.php');
?>
