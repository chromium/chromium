<?
header('ETag: foo');
header('Cache-control: max-age=0');

if ($_GET['type'] == 'css') {
    header('Content-Type: text/css');
}

if ($_SERVER['HTTP_IF_NONE_MATCH'] == 'foo') {
    header('HTTP/1.1 304 Not Modified');
    exit;
}
?>
foo
