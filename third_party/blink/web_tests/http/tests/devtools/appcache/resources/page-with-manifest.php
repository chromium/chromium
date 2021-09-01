<?php
$manifestId = $_GET["manifestId"];
echo "<html manifest=\"manifest.php?manifestId=" . $manifestId . "\">\n";
?>
<head>
</head>
<body>
<?php
$manifestId = $_GET["manifestId"];
echo "Page with manifest #" . $manifestId. ".\n";
?>
</body>
</html>
