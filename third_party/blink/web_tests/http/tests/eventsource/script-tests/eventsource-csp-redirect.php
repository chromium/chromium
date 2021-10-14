<?php
    header("Content-Security-Policy: connect-src http://127.0.0.1:8000/js-test-resources/js-test.js http://127.0.0.1:8000/resources/redirect.php", false);
    header("Content-Type: application/javascript", false);
?>

if (self.importScripts)
    importScripts("/js-test-resources/js-test.js");

description("Test that basic EventSource cross-origin requests fail on blocked CSP redirect.");

self.jsTestIsAsync = true;

var es;

shouldNotThrow("es = new EventSource(\"http://127.0.0.1:8000/resources/redirect.php?code=307&cors_allow_origin=*&url=http://127.0.0.1:8080/resources/redirect.php\")");
es.onerror = function() {
    shouldBe("es.readyState", "EventSource.CLOSED");
    finishJSTest();
};
