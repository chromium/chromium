// META: script=resources/support.js
// META: script=resources/ports.sub.js
//
// Spec: https://wicg.github.io/private-network-access/#integration-fetch
//
// These tests verify that websocket connections behave similarly to fetches.
//
// This file covers only those tests that must execute in a secure context.
// Other tests are defined in: websocket.https.window.js

setup(() => {
  // Making sure we are in a secure context, as expected.
  assert_true(window.isSecureContext);
});

promise_test(t => websocketTest(t, {
  source: {
    protocol: "https:",
    port: kPorts.httpsLocal,
  },
  target: {
    protocol: "wss:",
    port: kPorts.wssLocal,
  },
  expected: kWebsocketTestResult.success,
}), "local to local: websocket success.");

promise_test(t => websocketTest(t, {
  source: {
    protocol: "https:",
    port: kPorts.httpsPrivate,
  },
  target: {
    protocol: "wss:",
    port: kPorts.wssLocal,
  },
  expected: kWebsocketTestResult.success,
}), "private to local: websocket success.");

promise_test(t => websocketTest(t, {
  source: {
    protocol: "https:",
    port: kPorts.httpsPublic,
  },
  target: {
    protocol: "wss:",
    port: kPorts.wssLocal,
  },
  expected: kWebsocketTestResult.success,
}), "public to local: websocket success.");

promise_test(t => websocketTest(t, {
  source: {
    protocol: "https:",
    port: kPorts.httpsLocal,
    treatAsPublicAddress: true,
  },
  target: {
    protocol: "wss:",
    port: kPorts.wssLocal,
  },
  expected: kWebsocketTestResult.success,
}), "treat-as-public to local: websocket success.");
