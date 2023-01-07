<?php
header('Content-Type: application/webbundle');
header('X-Content-Type-Options: nosniff');
echo file_get_contents('webbundle.wbn');
?>

