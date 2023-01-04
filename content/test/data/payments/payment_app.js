// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('abortpayment', e => {
  e.respondWith(true);
});

self.addEventListener('canmakepayment', e => {
  // Note that the following postMessage operations are not normal usage
  // in CanMakePaymentEvent. They are only used for testing purpose.
  // Please see content/browser/payments/payment_app_browsertest.cc
  // (PaymentAppBrowserTest.CanMakePayment)
  e.waitUntil(clients.matchAll({includeUncontrolled: true}).then(clients => {
    clients.forEach(client => {
      if (client.url.indexOf('payment_app_invocation.html') != -1) {
        client.postMessage(e.topOrigin);
        client.postMessage(e.paymentRequestOrigin);
        client.postMessage(JSON.stringify(e.methodData));
        client.postMessage(JSON.stringify(e.modifiers));
      }
    });
  }));

  e.respondWith(true);
});

self.addEventListener('paymentrequest', e => {
  e.waitUntil(clients.matchAll({includeUncontrolled: true}).then(clients => {
    clients.forEach(client => {
      if (client.url.indexOf('payment_app_invocation.html') != -1) {
        client.postMessage(e.topOrigin);
        client.postMessage(e.paymentRequestOrigin);
        client.postMessage(e.paymentRequestId);
        client.postMessage(JSON.stringify(e.methodData));
        client.postMessage(JSON.stringify(e.total));
        client.postMessage(JSON.stringify(e.modifiers));
        client.postMessage(e.instrumentKey);
      }
    });
  }));

  // SW -------------------- openWindow() ------------------> payment_app_window
  // SW <----- postMessage('payment_app_window_ready') ------ payment_app_window
  // SW -------- postMessage('payment_app_request') --------> payment_app_window
  // SW <-- postMessage({methodName: 'test', details: {}}) -- payment_app_window
  e.respondWith(new Promise((resolve, reject) => {
    let payment_app_window = undefined;
    let window_ready = false;

    let maybeSendPaymentRequest = function() {
      if (window_ready)
        payment_app_window.postMessage('payment_app_request');
    };

    self.addEventListener('message', e => {
      if (e.data == "payment_app_window_ready") {
        window_ready = true;
        maybeSendPaymentRequest();
        return;
      }

      if (e.data.methodName) {
        resolve(e.data);
        return;
      }
    });

    // Open a window for the payment instrument.
    var payment_app_web_page = 'payment_app_window.html';
    if(e.instrumentKey == 'bobpay-payment-app-id') {
      payment_app_web_page = 'https://bobpay.test';
    }
    e.openWindow(payment_app_web_page)
      .then(window_client => {
        payment_app_window = window_client;
        if(payment_app_window == null) {
          reject('failed to openWindow');
          return;
        }
        maybeSendPaymentRequest();
      })
      .catch(error => {
        reject(error);
      });
  }));
});
