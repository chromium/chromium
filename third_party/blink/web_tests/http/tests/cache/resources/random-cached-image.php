<?php
# The script name is "random-cached-image" because this is expected to return
# different images on every (re)load to test caching, which is similar to
# other random-cached* scripts.
# However, this actually returns an image from a small number of predefined
# images in sequence,
# because it is hard to generate random (PNG/JPEG/etc.) images from scratch.

require_once '../../resources/portabilityLayer.php';

if (!sys_get_temp_dir()) {
    echo "FAIL: No temp dir was returned.\n";
    exit();
}

$id = $_GET['id'];
if (!is_string($id) || !preg_match("/^[a-z0-9-]+$/", $id)) {
    echo "FAIL: invalid id.\n";
    exit();
}

$countFilename = sys_get_temp_dir() . "/random-cached-image." . $id . ".tmp";
$count = 0;
if (file_exists($countFilename)) {
    $count = file_get_contents($countFilename);
}
$count += 1;
file_put_contents($countFilename, $count);

# Images with different dimensions.
$imageFilenames = array(
  '../../resources/square20.png',
  '../../resources/square100.png',
  '../../resources/square200.png'
);

header("Content-type: image/png");
header("Cache-control: max-age=60000");
header("ETag: 98765");

readfile($imageFilenames[$count % count($imageFilenames)]);
?>
