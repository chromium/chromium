<?php
header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");
header('HTTP/1.1 307 Temporary Redirect');
header('Location: resources/access-via-redirect.html');
?>
