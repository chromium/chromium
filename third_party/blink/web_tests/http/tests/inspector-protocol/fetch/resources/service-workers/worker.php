<?php
  // To test Service Worker Update behavior, set "defer" to a unique value/session_id.
  // With this set, the first request will return a working Service Worker main script.
  // Subsequent calls will fall through and respect the other parameters of PHP page.
  $defer = $_GET["defer"];
  if (isset($defer)) {
    $sid = $_GET["defer"];
    session_id($sid);
    session_start();
    $visited = $_SESSION["visited"];

    if (!isset($visited)) {
      $_SESSION["visited"] = true;
      session_commit();

      header("Content-Type: text/javascript");
      echo "console.log('hi from service worker…your next request here will return different worker code!');";
      die();
    }
  }

  // Redirect Response
  $redirect_to = $_GET["redirect_to"];
  if (isset($redirect_to)) {
    header('Location: ' . $redirect_to);
    die();
  }

  // Allow manipulation of other fields that can induce Service Worker failures
  $content_type = $_GET["content_type"] ?? "text/javascript";
  $response_code = $_GET["response_code"] ?? 200;
  $service_worker_allowed_header = $_GET["service_worker_allowed_header"];

  http_response_code($response_code);
  header("Content-Type: " . $content_type);
  if (isset($service_worker_allowed_header)) {
    header("Service-Worker-Allowed: " . $service_worker_allowed_header);
  }

  echo "console.log('hi from the service worker!')";
?>
