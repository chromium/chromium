<?php
    if (isset($_SERVER["HTTP_IF_MODIFIED_SINCE"])) {
        header("HTTP/1.0 304 Not Modified");
        exit;
    }

    header("Cache-control: max-age=3600");
    header("Expires: " . gmdate(DATE_RFC1123, time() + 600));
    header("Last-Modified: " . gmdate(DATE_RFC1123, time() - 600));
    header("Content-Type:text/javascript; charset=UTF-8");
?>
console.log("Done.");
