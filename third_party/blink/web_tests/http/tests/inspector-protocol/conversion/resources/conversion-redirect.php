<?php
header('HTTP/1.1 302 Found');
header('Location: /.well-known/attribution-reporting/trigger-attribution?' . http_build_query($_GET));
?>
