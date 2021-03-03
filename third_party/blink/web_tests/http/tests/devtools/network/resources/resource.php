<?php
    $type = $_GET["type"];
    $wait = $_GET["wait"];
    $tail_wait = $_GET["tail_wait"];
    $send = $_GET["send"];
    $size = $_GET["size"];
    $gzip = $_GET["gzip"];
    $jsdelay = $_GET["jsdelay"];
    $jscontent = $_GET["jscontent"];
    $chunked = $_GET["chunked"];
    $random = $_GET["random"];
    $cached = $_GET["cached"];
    $nosniff = $_GET["nosniff"];
    $download = $_GET["download"];
    $named_download = $_GET["named_download"];
    $mime_type = $_GET["mime_type"];
    $body_pattern = $_GET["body_pattern"];

    # Wait before sending response
    if ($wait)
        usleep($wait * 1000);

    # Exit early if we return 304 code.
    if ($cached && $_SERVER["HTTP_IF_MODIFIED_SINCE"]) {
        header("HTTP/1.0 304 Not Modified");
        exit;
    }

    # Enable gzip compression if needed
    if ($gzip)
        ob_start("ob_gzhandler");

    # Send headers
    if ($cached) {
        $max_age = 12 * 31 * 24 * 60 * 60; //one year
        $expires = gmdate(DATE_RFC1123, time() + $max_age);
        $last_modified = gmdate(DATE_RFC1123, time() - $max_age);

        header("Cache-Control: public, max-age=" . 5*$max_age);
        header("Cache-control: max-age=0");
        header("Expires: " . $expires);
        header("Last-Modified: " . $last_modified);
    } else {
        header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
        header("Cache-Control: no-store, no-cache, must-revalidate");
        header("Pragma: no-cache");
    }
    if ($type == "js")
        header("Content-Type:text/javascript; charset=UTF-8");
    else if ($type == "image")
        header("Content-Type: image/png");
    else
        header("Content-Type: text/plain");

    if ($nosniff)
        header("x-content-type-options: nosniff");

    if ($download)
        header("Content-Disposition: attachment; filename=hello.txt");

    if (isset($named_download)) {
        $filename_part = "";
        if ($named_download !== "") {
            $filename_part = "; filename=" . $named_download;
        }
        header("Content-Disposition: attachment" . $filename_part);
    }

    if ($mime_type)
        header("Content-type: " . $mime_type);

    # Flush headers and sleep bofore sending response
    if ($send) {
        flush();
        usleep($send * 1000);
    }

    if ($type == "js") {
        # Send JavaScript file
        $bytes_emitted = 0;
        if ($jsdelay) {
            # JavaScript file should block on load
?>
function __foo(seconds) {
    var now =  Date.now();
    var counter = Number(0);
    while(now + seconds  > Date.now()) { counter = Number(counter + 1); }
}
__foo(<?php echo($jsdelay)?>);
<?php
            $bytes_emitted += 175;
        }

        if (!$jscontent)
            $jscontent = "function foo() {}";

        # JavaScript file should issue given command.
        echo($jscontent);
        $bytes_emitted += strlen($jscontent);

        if ($size) {
            for ($i = 0; $i < $size - $bytes_emitted; ++$i)
                echo("/");
        }
    } else if ($type == "image") {
        $base64data = "iVBORw0KGgoAAAANSUhEUgAAACAAAAAYCAYAAACbU/80AAAAAXNSR0IArs4c6QAAAAlwSFlzAAALEwAACxMBAJqcGAAAAAd0SU1FB9kICQsw" .
            "ARCJCogAAABFSURBVEjH7ZLBCQAwCAONdP+V0xVqq0gx9w+Gi2ZCTAcXGWbe8G4Dq9DekS" .
            "kPaGeFgfYJVODlCTnWADILoEg3vplACLEBN9UGG9+mxboAAAAASUVORK5CYII=";
        $data = base64_decode($base64data);
        $data_len = strlen($data);
        print($data);
        if ($size) {
            if ($chunked) {
                ob_flush();
                flush();
            }
            for ($i = 0; $size && $i < $size - $data_len; ++$i)
                echo("=");
        } else if ($random)
            echo(rand());
    } else {
        # Generate dummy text/html.
        if ($size) {
            $str = $body_pattern ? $body_pattern : "*";
            for ($i = 0; $i < $size; ++$i) {
                if ($chunked && (1 == $i)) {
                    ob_flush();
                    flush();
                }
                echo($str[$i % strlen($str)]);
            }
        } else {
            echo("Hello ");
            if ($chunked) {
                ob_flush();
                flush();
            }
            echo("world");
            if ($random)
                echo(": " . rand());
        }
    }
    # Useful in some download-related tests
    if ($tail_wait) {
        flush();
        ob_flush();
        usleep($tail_wait * 1000);
    }
?>
