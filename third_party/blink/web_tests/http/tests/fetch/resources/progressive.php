<?php
function put_chunk($txt) {
  echo sprintf("%x\r\n", strlen($txt));
  echo "$txt\r\n";
}

header("Content-type: application/octet-stream");
header("Transfer-encoding: chunked");
flush();

for ($i = 0; $i < 100; $i++) {
  put_chunk("$i");
  if (ob_get_level() > 0){
    ob_flush();
  }
  flush();
  usleep(1000);
}
echo "0\r\n\r\n";
?>
