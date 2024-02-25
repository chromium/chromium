<?
// Returns response headers that cause revalidation, and for revalidating
// requests returns 200 and a body different from the original one.
header('ETag: foo');
header('Cache-control: max-age=0');

$type = $_GET['type'] ?? null;
if ($type == 'css') {
    header('Content-Type: text/css');
}
$http_if_none_match = $_SERVER['HTTP_IF_NONE_MATCH'] ?? null;
if ($http_if_none_match == 'foo') {
    // The body after revalidation.
    echo "/* after revalidation */";
    exit;
}
// The body before revalidation.
echo "/* comment */";
