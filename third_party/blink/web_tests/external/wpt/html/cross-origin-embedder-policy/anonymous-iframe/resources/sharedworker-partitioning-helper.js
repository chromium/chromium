let messages = {};

onconnect = function(e) {
  let port = e.ports[0];

  port.addEventListener('message', function(e) {
    const action = e.data.action;
    const from = e.data.from;

    if (action === 'record') {
      messages[from] = true;
    }

    if (action === 'retrieve') {
      port.postMessage(messages);
    }
  });

  port.start();
};
