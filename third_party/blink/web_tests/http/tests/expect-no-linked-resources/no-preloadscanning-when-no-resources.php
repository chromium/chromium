<?php
header("Document-Policy: expect-no-linked-resources");
?>
<!DOCTYPE html>
<meta charset="utf-8">
<title>Preloads are not identified when Document Policy expect-no-linked-resources is enabled</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script>
  var EXPECT_NO_LINKED_RESOURCES_COUNTER = 5100;
  test(t => {
    assert_false(internals.isPreloaded('resources/square20.png'), 'image was not preloaded');
    assert_true(internals.isUseCounted(document, EXPECT_NO_LINKED_RESOURCES_COUNTER));
  }, 'Resources are not preloaded when expect-no-linked-resources feature is enabled.');
</script>
<div>This test passes if the img src is not preloaded when expect-no-linked-resources feature is enabled.</div>
<img src="resources/square20.png">
</body>
</html>