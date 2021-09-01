<?php
# This resource won't be cached by network layer, but will be cached by appcache.
header("Last-Modified: Thu, 01 Dec 2003 16:00:00 GMT");
header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, no-store");
header("Pragma: no-cache");
header("Content-Type: text/plain");

if ($_SERVER['HTTP_IF_MODIFIED_SINCE'] || $_SERVER['HTTP_IF_NONE_MATCH'])
    header("HTTP/1.1 304 Not Modified");
else
    print("Hello, world!\n");
?>
