<?php
    header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
    header("Cache-Control: no-store, no-cache, must-revalidate");
    header("Pragma: no-cache");
    header("Content-Type: text/plain");
    header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
    $length = isset($_GET["length"]) ? (int) $_GET["length"] : 0;
    if (!$length) {
      setcookie("TestCookie", "TestCookieValue");
      setcookie("TestCookie2", "TestCookieValue2");
    } else {
      $data = "";
      for ($i = 0; $i < $length; $i++) {
        $data .= "a";
      }
      header("Set-Cookie: $data");
      $data = "";
      for ($i = 0; $i < $length; $i++) {
        $data .= "b";
      }
      header("Set-Cookie: $data");
    }
    echo("Cookie set.");
?>
