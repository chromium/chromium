<?php
    $HTTP_IF_NON_MATCH = $_SERVER["HTTP_IF_NONE_MATCH"] ?? null;
    if ($HTTP_IF_NON_MATCH) {
        header("HTTP/1.0 304 Not Modified");
        header("Access-Control-Allow-Origin: *");
        exit;
    }

    header("ETag: \"ABC\"");
    header("Access-Control-Allow-Origin: *");
    header("Cache-Control: max-age=0;must-revalidate");
    header("Content-Type: text/html; charset=UTF-8");
?>
<html>
<body>
Today's lucky number: <?php echo(rand()) ?>
</body>
</body>
