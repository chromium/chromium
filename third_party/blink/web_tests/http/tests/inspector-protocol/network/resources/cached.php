<?php
    $HTTP_IF_MODIFIED_SINCE = $_SERVER["HTTP_IF_MODIFIED_SINCE"] ?? null;
    if ($HTTP_IF_MODIFIED_SINCE) {
        header("HTTP/1.0 304 Not Modified");
        exit;
    }

    header("Cache-control: max-age=360000");
    header("Expires: " . gmdate(DATE_RFC1123, time() + 60000));
    header("Last-Modified: " . gmdate(DATE_RFC1123, time() - 60000));
    header("Content-Type: text/html; charset=UTF-8");
?>
<html>
<body>
Today's lucky number: <?php echo(rand()) ?>
</body>
</body>
