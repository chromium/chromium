// META: script=resources/support.js
// META: script=resources/ports.sub.js
//
// Spec: https://wicg.github.io/private-network-access/#integration-fetch

// These tests verify that websocket connections behave similarly to fetches.
//
// This file covers only those tests that must execute in a non secure context.
// Other tests are defined in: websocket.https.window.js

setup(() => {
  // Making sure we are in a non secure context, as expected.
  assert_false(window.isSecureContext);
});

promise_test(t => websocketTest(t, {
  source: {
    port: kPorts.httpLocal,
  },
  target: {
    protocol: "ws:",
    port: kPorts.wsLocal,
  },
  expected: kWebsocketTestResult.success,
}), "local to local: websocket success.");

promise_test(t => websocketTest(t, {
  source: {
    port: kPorts.httpPrivate,
  },
  target: {
    protocol: "ws:",
    port: kPorts.wsLocal,
  },
  expected: kWebsocketTestResult.failure,
}), "private to local: websocket failure.");

promise_test(t => websocketTest(t, {
  source: {
    port: kPorts.httpPublic,
  },
  target: {
    protocol: "ws:",
    port: kPorts.wsLocal,
  },
  expected: kWebsocketTestResult.failure,
}), "public to local: websocket failure.");

promise_test(t => websocketTest(t, {
  source: {
    port: kPorts.httpLocal,
    treatAsPublicAddress: true,
  },
  target: {
    protocol: "ws:",
    port: kPorts.wsLocal,
  },
  expected: kWebsocketTestResult.failure,
}), "treat-as-public to local: websocket failure.");
