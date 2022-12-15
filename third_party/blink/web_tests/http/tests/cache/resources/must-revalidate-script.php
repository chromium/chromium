<?php
$last_modified = gmdate(DATE_RFC1123, time() - 1);

header('Cache-Control: private, max-age=0, must-revalidate');
header('Last-Modified: ' . $last_modified);
header('Content-Type: application/javascript');
echo('must_revalidate_script_id = "' . uniqid() . '";');
?>
