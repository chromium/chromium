<?php
  header("Access-Control-Allow-Origin: *");

  // Extract the cookie information.
  $headers = getallheaders();
  $cookieHeader = $headers['Cookie'];

  // Append the cookieHeader to the destination url
  // as a query parameter.
  $url = $_GET['location'];
  $urlWithQuery = $url . "?cookie=" . strval($cookieHeader);

  // Perform the redirect.
  header("Location: $urlWithQuery", TRUE, 302);
?>