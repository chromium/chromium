<?php

function getReferrerPath() {
    if (!isset($_SERVER["HTTP_REFERER"]))
        return "";
    $url = parse_url($_SERVER["HTTP_REFERER"]);
    return $url['path'];
}

function putImage() {
    $image = "../../resources/square100.png";
    header("Content-Type: image/png");
    header("Content-Length: " . filesize($image));
    header("Access-Control-Allow-Origin: *");
    if (ob_get_length() > 0) {
        ob_clean();
    }
    flush();
    readfile($image);
}

function putFont() {
    $font = "../../resources/Ahem.ttf";
    header("Content-Type: font/truetype");
    header("Content-Length: " . filesize($font));
    header("Access-Control-Allow-Origin: *");
    if (ob_get_length() > 0) {
        ob_clean();
    }
    flush();
    readfile($font);
}

$expectedReferrerPaths = array(
    "document" => "/css/css-resources-referrer.html",
    "sheet" => "/css/resources/css-resources-referrer.css",
    "importedSheet" => "/css/resources/css-resources-referrer-import.css",
    "iframe" => "/css/css-resources-referrer-srcdoc.html"
);

$from = $_GET["from"];
$resource = $_GET["resource"];
$referrerPath = getReferrerPath();

if ($referrerPath === $expectedReferrerPaths[$from]) {
    if ($resource === "image" || $resource === "image2")
        putImage();
    else if ($resource === "font")
        putFont();
    else
        header("HTTP/1.1 500 Internal Server Error");
} else {
    header("HTTP/1.1 500 Internal Server Error");
}

?>
