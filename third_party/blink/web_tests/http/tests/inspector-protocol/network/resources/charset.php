<?php
    $test = $_GET["test"] ?? "utf-8";
    $data = [
        "windows-1251" => base64_decode("9eXr6+7z"),
        "utf-8" => "Hello",
        "utf-8-mb" => "123456789123456789ࠀ", // 21 bytes
        "binary-utf8" => "Hello",
        "binary" => base64_decode("QYvK9TIXnCdasXe2xNs59egguscqE1BaK/Ybba7Z2Ac="),
    ];
    if ($test == "binary-utf8" || $test == "binary") {
      header("Content-Type: application/octet-stream");
    } else {
      header("Content-Type: text/plain; charset=" . $test);
    }
    echo $data[$test] ?? "not  found";
?>
