<?php

if (isset($_SERVER["HTTP_IF_MODIFIED_SINCE"])) {
    header("HTTP/1.1 304 Not Modified");
} else {
    $font = "../../resources/Ahem.ttf";

    header("Cache-Control: public, max-age=86400");
    header('Last-Modified: ' . gmdate("D, d M Y H:i:s", filemtime($font)) . " GMT");
    header("Content-Type: font/truetype");
    header("Content-Length: " . filesize($font));
    header("Access-Control-Allow-Origin: *");
    header("Timing-Allow-Origin: *");
    if (ob_get_length() > 0) {
        ob_clean();
    }
    flush();
    readfile($font);
}

?>
