<?php
// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that payment feature when enabled for self works in
// the same origin or when allowpaymentrequest is set. No cross-origin iframe
// may call it when allowpaymentrequest is not set.

Header("Feature-Policy: payment 'self'");
?>

<!DOCTYPE html>
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>
<script src="resources/helper.js"></script>
<iframe></iframe>
<iframe allowpaymentrequest></iframe>
<script>
var srcs = [
  "resources/feature-policy-payment.html",
  "http://localhost:8000/feature-policy/resources/feature-policy-payment.html"
];

function loadFrame(iframe, src) {
  var allowpaymentrequest = iframe.hasAttribute('allowpaymentrequest');
  promise_test(function() {
    iframe.src = src;
    return new Promise(function(resolve, reject) {
      window.addEventListener('message', function(e) {
        resolve(e.data);
      }, { once: true });
    }).then(function(data) {
      // paymentrequest is enabled if same origin, and blocked if not,
      // regardless of the allowpaymentrequest attribute.
      if (src === srcs[0]) {
        assert_true(data.enabled, 'Paymentrequest():');
      } else {
        assert_false(data.enabled, 'Paymentrequest():');
        assert_equals(data.name, 'SecurityError', 'Exception Name:');
        assert_equals(data.message, "Failed to construct 'PaymentRequest': " +
          "Must be in a top-level browsing context or an iframe needs to " +
          "specify allow=\"payment\" explicitly", 'Error Message:');
      }
    });
  }, 'Paymentrequest enabled for self on URL: ' + src + ' with '+
    'allowpaymentrequest = ' + allowpaymentrequest);
}

window.onload = function() {
  loadIframes(srcs);
}
</script>
