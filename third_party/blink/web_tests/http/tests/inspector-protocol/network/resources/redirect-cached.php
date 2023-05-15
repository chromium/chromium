<?php
header('HTTP/1.1 307 Temporary Redirect');
header('Pragma: no-cache');
header('Location: redirect-cached-target.php');
?>