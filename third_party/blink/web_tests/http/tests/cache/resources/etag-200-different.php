<?
// Returns response headers that cause revalidation, and for revalidating
// requests returns 200 and a body different from the original one.
header('ETag: foo');
header('Cache-control: max-age=0');

if ($_GET['type'] == 'css') {
    header('Content-Type: text/css');
}

if ($_SERVER['HTTP_IF_NONE_MATCH'] == 'foo') {
    // The body after revalidation.
    echo "/* after revalidation */";
    exit;
}
// The body before revalidation.
echo "/* comment */";
