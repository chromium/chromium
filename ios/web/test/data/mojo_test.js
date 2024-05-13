// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module provides the test page for WebUIMojoTest. Once the page is
// loaded it sends "syn" message to the native code. Once page receives "ack"
// from the native code, the page then sends "fin". Test succeeds only when
// "fin" is received by the native page. Refer to
// ios/web/webui/web_ui_mojo_inttest.mm for testing code.

var pageImpl, browserProxy;

/** @constructor */
function TestPageImpl() {
  this.binding = new mojo.Binding(web.mojom.TestPage, this);
}

TestPageImpl.prototype = {
  /** @override */
  handleNativeMessage: function(result) {
    if (result.message == 'ack') {
      // Expect "ack2" reply to syn2 via the callback.
      browserProxy.handleJsMessageWithCallback('syn2').then((r) =>
      {
        if (r.result.message == 'ack2') {
          // Send "fin" to complete the test.
          browserProxy.handleJsMessage('fin');
        }
      });
    }
  },
};

/**
 * @return {!Promise} Fires when DOMContentLoaded event is received.
 */
function whenDomContentLoaded() {
  return new Promise(function(resolve, reject) {
    document.addEventListener('DOMContentLoaded', resolve);
  });
}

function main() {
  whenDomContentLoaded().then(function() {
    browserProxy = new web.mojom.TestUIHandlerMojoPtr();
    Mojo.bindInterface(web.mojom.TestUIHandlerMojo.name,
                       mojo.makeRequest(browserProxy).handle);

    pageImpl = new TestPageImpl();
    var pagePtr = new web.mojom.TestPagePtr();
    pageImpl.binding.bind(mojo.makeRequest(pagePtr));
    browserProxy.setClientPage(pagePtr);

    // Send "syn" so native code should reply with "ack".
    browserProxy.handleJsMessage('syn');
  });
}

main();
