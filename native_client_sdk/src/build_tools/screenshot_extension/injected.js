// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Only inject once.
if (!document.body.getAttribute('screenshot_extension_injected')) {
  document.body.setAttribute('screenshot_extension_injected', true);
  (function() {

    // Bounce message from webpage to background page.
    //
    // Expecting a message called with:
    // window.postMessage({
    //    id: <a value that is passed back unchanged to the response for
    //         identification>,
    //    target: 'background'
    // }, '*');
    //
    // When the screenshot is captured, a message will be posted to the window.
    // Listen for it like this:
    //
    // window.addEventListener('message', function(event) {
    //   if (event.source !== window)
    //     return;
    //
    //   if (event.data.target !== 'page')
    //     return;
    //
    //   // event.data is an object:
    //   // {
    //   //   id: <the id passed to the request>,
    //   //   target: 'page',
    //   //   data: <a data URI of MIMEtype image/png with the tab screenshot>
    //   // }
    //   //
    //   // or if there is an error:
    //   //
    //   // {
    //   //   id: <the id passed to the request>,
    //   //   target: 'page',
    //   //   error: <an error string>
    //   // }
    // }, false);
    //
    window.addEventListener('message', function(event) {
      if (event.source !== window)
        return;

      // Ignore messages not destined for the background page.
      if (event.data.target !== 'background')
        return;

      var id = event.data.id;
      console.log('sending message: id=' + id);

      chrome.runtime.sendMessage(null, {},
          function(responseData) {
            // Bounce response from background page back to webpage.
            var lastError = chrome.runtime.lastError;
            if (lastError) {
              console.log('lastError: ' + lastError);

              window.postMessage({id: id, target: 'page', error: lastError},
                                 '*');
              return;
            }

            console.log('received response: id=' + id);

            window.postMessage({id: id, target: 'page', data: responseData},
                               '*');
      });
    }, false);
  })();
}
