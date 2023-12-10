<?php
$url = $_GET['Redirect'] ?? null;
$path = '/fetch/resources/redirect-loop.php';
if (isset($_GET['Count'])) {
  $count = intval($_GET['Count']) - 1;
  if ($count > 0) {
    $url = $path .
           '?Redirect=' . rawurlencode($url);
    if (isset($_GET['ACAOrigin']))
      $url .= '&ACAOrigin=' . $_GET['ACAOrigin'];
    $url .= '&Count=' . $count ;
  }
}
header("Location: $url");
if (isset($_GET['ACAOrigin'])) {
  $origins = explode(',', $_GET['ACAOrigin']);
  for ($i = 0; $i < sizeof($origins); ++$i)
    header("Access-Control-Allow-Origin: " . $origins[$i], false);
}
?>
