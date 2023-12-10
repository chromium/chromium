<?
header('ETag: foo');
header('Cache-control: max-age=0');

$type = $_GET['type'] ?? null;
if ($type == 'css') {
    header('Content-Type: text/css');
}

$http_if_none_match = $_SERVER['HTTP_IF_NONE_MATCH'] ?? null;
if ($http_if_none_match == 'foo') {
    header('HTTP/1.1 304 Not Modified');
    exit;
}
?>
foo
