<?php
    if (isset($_SERVER["HTTP_IF_MODIFIED_SINCE"])) {
        header("HTTP/1.0 304 Not Modified");
        exit;
    }

    $max_age = 12 * 31 * 24 * 60 * 60; //one year
    $expires = gmdate(DATE_RFC1123, time() + $max_age);
    $last_modified = gmdate(DATE_RFC1123, time() - $max_age);

    header("Cache-Control: public, max-age=" . 5*$max_age);
    header("Cache-control: max-age=0");
    header("Expires: " . $expires);
    header("Content-Type: text/html");
    header("Last-Modified: " . $last_modified);

    echo("console.log(\"Done.\");");
    echo("var randomValue = " . rand() . ";");
?>
