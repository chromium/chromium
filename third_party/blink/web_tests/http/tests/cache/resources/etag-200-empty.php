<?
// Returns non-empty body and response headers that cause revalidation, and
// returns 200 with empty body for revalidating requests.
header('ETag: foo');
header('Cache-control: max-age=0');

if ($_GET['type'] == 'css') {
    header('Content-Type: text/css');
}

if ($_SERVER['HTTP_IF_NONE_MATCH'] == 'foo') {
    // The body is intentionally empty.
    exit;
}
echo "/* comment */";
?>
