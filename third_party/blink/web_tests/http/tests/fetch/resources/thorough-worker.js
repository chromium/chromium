importScripts('/resources/get-host-info.js?pipe=sub');
importScripts('fetch-test-options.js');
importScripts('thorough-util.js');

var port = undefined;
var isTestTargetFetch = false;

self.onmessage = function(e) {
  var message = e.data;
  if ('port' in message) {
    port = message.port;
  } else if (message.msg === 'START TEST CASE') {
    isTestTargetFetch = true;
    port.postMessage({msg: 'READY'});
  }
};

self.addEventListener('fetch', function(event) {
    if (!isTestTargetFetch) {
      // Don't handle the event when it is not the test target fetch such as a
      // redirected fetch or for the iframe html.
      return;
    }
    isTestTargetFetch = false;

    event.respondWith(
      doFetch(event.request)
        .then(function(message) {
            var response = message.response;
            message.response = undefined;
            // Send the result to |thorough-util.js|.
            port.postMessage(message);
            return response;
          })
        .catch(function(message) {
            port.postMessage(message);
            return Promise.reject();
          })
      );
  });
