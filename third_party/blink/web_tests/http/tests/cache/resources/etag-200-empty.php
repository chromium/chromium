<?
// Returns non-empty body and response headers that cause revalidation, and
// returns 200 with empty body for revalidating requests.
header('ETag: foo');
header('Cache-control: max-age=0');
$type = $_GET['type'] ?? null;
if ($type == 'css') {
    header('Content-Type: text/css');
}

$http_if_none_match = $_SERVER['HTTP_IF_NONE_MATCH'] ?? null;
if ($http_if_none_match == 'foo') {
    // The body is intentionally empty.
    exit;
}
echo "/* comment */";
?>
