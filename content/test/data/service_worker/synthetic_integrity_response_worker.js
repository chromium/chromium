// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kSignatureInput = 'sig=("unencoded-digest";sf);' +
    'keyid="JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";' +
    'tag="ed25519-integrity"';

const kValidSignature =
    'sig=:7gD6QAEl17qHfl/RMbS0pmEP7LYwjKLI2n66X1l4vRrTgZAPuj3Z157M2dTp6t3' +
    'pqiH2q4TAeR/GkSwXb/OADA==:';

const kInvalidSignature =
    'sig=:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' +
    'AAAAAAAAAAAAAAAAAAAA==:';

this.onfetch = function(event) {
  if (event.request.url.indexOf('with_valid_sri') !== -1) {
    event.respondWith(
        new Response('window.__sri_service_worker_executed = true;', {
          headers: {
            'Content-Type': 'application/javascript',
            'Signature-Input': kSignatureInput,
            'Signature': kValidSignature,
            'Unencoded-Digest':
                'sha-256=:vnNzg9/ED6PkjuTQ6evvC0fgI3d+DxdkS7YVrsV93L4=:'
          }
        }));
  } else if (event.request.url.indexOf('with_sri') !== -1) {
    event.respondWith(
        new Response('console.log(\'synthetic script loaded\');', {
          headers: {
            'Content-Type': 'application/javascript',
            'Signature-Input': kSignatureInput,
            'Signature': kInvalidSignature,
            'Unencoded-Digest':
                'sha-256=:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=:'
          }
        }));
  } else if (event.request.url.indexOf('integrity_test.js') !== -1) {
    event.respondWith(new Response(
        'console.log(\'normal script loaded\');',
        {headers: {'Content-Type': 'application/javascript'}}));
  }
};
