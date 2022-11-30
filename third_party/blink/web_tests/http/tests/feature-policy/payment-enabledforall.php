<?php
// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures that payment feature when enabled for all works across
// all origins regardless whether allowpaymentrequest is set. (Feature policy
// header takes precedence over the absence of allowpaymentrequest.)

Header("Feature-Policy: payment *");
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
      // paymentrequest is enabled if:
      //     a. same origin; or
      //     b. enabled by allowpaymentrequest.
      if (src === srcs[0] || allowpaymentrequest) {
        assert_true(data.enabled, 'Paymentrequest():');
      } else {
        assert_false(data.enabled, 'Paymentrequest():');
        assert_equals(data.name, 'SecurityError', 'Exception Name:');
        assert_equals(data.message, "Failed to construct 'PaymentRequest': " +
          "Must be in a top-level browsing context or an iframe needs to " +
          "specify allow=\"payment\" explicitly", 'Error Message:');
      }
    });
  }, 'Paymentrequest enabled for all on URL: ' + src + ' with ' +
    'allowpaymentrequest = ' + allowpaymentrequest);
}

window.onload = function() {
  loadIframes(srcs);
}
</script>
