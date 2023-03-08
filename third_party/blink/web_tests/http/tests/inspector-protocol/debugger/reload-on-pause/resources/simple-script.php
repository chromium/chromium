<!DOCTYPE html>
<html lang="en">
<body>
<?php if ($_GET['type'] == 'module'): ?>
  <script type="module" src="<?= $_GET['script'] ?>"></script>
<?php else: ?>
  <script src="<?= $_GET['script'] ?>"></script>
<?php endif; ?>
</body>
</html>
