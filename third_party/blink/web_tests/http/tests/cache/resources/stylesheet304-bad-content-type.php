<?php
require_once '../../resources/portabilityLayer.php';

clearstatcache();

if (isset($_SERVER["HTTP_IF_MODIFIED_SINCE"])) {
    header("HTTP/1.0 304 Not Modified");
    header("Content-Type: text/plain");
    exit();
}
$one_year = 12 * 31 * 24 * 60 * 60;
$last_modified = gmdate(DATE_RFC1123, time() - $one_year);
$expires = gmdate(DATE_RFC1123, time() + $one_year);

header('Cache-Control: public, max-age=' . $one_year);
header('Expires: ' . $expires);
header('Content-Type: text/css');
header('Etag: 123456789');
header('Last-Modified: ' . $last_modified);
?>
#test { background-color: rgb(0, 255, 0); }
