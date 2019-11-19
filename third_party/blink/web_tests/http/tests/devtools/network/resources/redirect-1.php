<?php
http_response_code(307);
header('x-devtools-redirect: 1');
header('location: http://redirect-two.test:8000/devtools/network/resources/redirect-2.php');
?>
