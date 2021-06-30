// META: script=resources/support.js
// META: script=resources/ports.sub.js
//
// Spec: https://wicg.github.io/private-network-access/#integration-fetch
//
// This file covers only those tests that must execute in a secure context.
// Other tests are defined in: non-secure-context.window.js

setup(() => {
  // Making sure we are in a secure context, as expected.
  assert_true(window.isSecureContext);
});

// These tests verify that secure contexts can fetch subresources from all
// address spaces.

promise_test(t => fetchTest(t, {
  source: { port: kPorts.httpsLocal },
  target: { port: kPorts.httpsLocal },
  expected: kFetchTestResult.success,
}), "Local secure context can fetch local subresource.");

promise_test(t => fetchTest(t, {
  source: { port: kPorts.httpsLocal },
  target: { port: kPorts.httpsPrivate },
  expected: kFetchTestResult.success,
}), "Local secure context can fetch private subresource.");

promise_test(t => fetchTest(t, {
  source: { port: kPorts.httpsLocal },
  target: { port: kPorts.httpsPublic },
  expected: kFetchTestResult.success,
}), "Local secure context can fetch public subresource.");

promise_test(t => fetchTest(t, {
  source: { port: kPorts.httpsPrivate },
  target: { port: kPorts.httpsLocal },
  expected: kFetchTestResult.success,
}), "Private secure context can fetch local subresource.");

promise_test(t => fetchTest(t, {
  source: { port: kPorts.httpsPrivate },
  target: { port: kPorts.httpsPrivate },
  expected: kFetchTestResult.success,
}), "Private secure context can fetch private subresource.");

promise_test(t => fetchTest(t, {
  source: { port: kPorts.httpsPrivate },
  target: { port: kPorts.httpsPublic },
  expected: kFetchTestResult.success,
}), "Private secure context can fetch public subresource.");

promise_test(t => fetchTest(t, {
  source: { port: kPorts.httpsPublic },
  target: { port: kPorts.httpsLocal },
  expected: kFetchTestResult.success,
}), "Public secure context can fetch local subresource.");

promise_test(t => fetchTest(t, {
  source: { port: kPorts.httpsPublic },
  target: { port: kPorts.httpsPrivate },
  expected: kFetchTestResult.success,
}), "Public secure context can fetch private subresource.");

promise_test(t => fetchTest(t, {
  source: { port: kPorts.httpsPublic },
  target: { port: kPorts.httpsPublic },
  expected: kFetchTestResult.success,
}), "Public secure context can fetch public subresource.");

// These tests verify that documents fetched from the `local` address space yet
// carrying the `treat-as-public-address` CSP directive are treated as if they
// had been fetched from the `public` address space.

promise_test(t => fetchTest(t, {
  source: {
    port: kPorts.httpsLocal,
    treatAsPublicAddress: true,
  },
  target: { port: kPorts.httpsLocal },
  expected: kFetchTestResult.success,
}), "Treat-as-public-address secure context can fetch local subresource.");

promise_test(t => fetchTest(t, {
  source: {
    port: kPorts.httpsLocal,
    treatAsPublicAddress: true,
  },
  target: { port: kPorts.httpsPrivate },
  expected: kFetchTestResult.success,
}), "Treat-as-public-address secure context can fetch private subresource.");

promise_test(t => fetchTest(t, {
  source: {
    port: kPorts.httpsLocal,
    treatAsPublicAddress: true,
  },
  target: { port: kPorts.httpsPublic },
  expected: kFetchTestResult.success,
}), "Treat-as-public-address secure context can fetch public subresource.");
