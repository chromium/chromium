<?php
    $request_origin_value = $_SERVER["HTTP_ORIGIN"];
    if (!is_null($request_origin_value)) {
        header("Access-Control-Allow-Origin: $request_origin_value");
        header("Access-Control-Allow-Credentials: true");
        header("Access-Control-Allow-Methods: GET,POST,OPTIONS");
    }
    echo json_encode($_POST);
?>
