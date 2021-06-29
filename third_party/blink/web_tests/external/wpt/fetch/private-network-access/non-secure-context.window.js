// META: script=resources/support.js
// META: script=resources/ports.sub.js
// META: script=resources/resolve_url.js
//
// Spec: https://wicg.github.io/private-network-access/#integration-fetch
//
// This file covers only those tests that must execute in a non secure context.
// Other tests are defined in: secure-context.window.js

setup(() => {
  // Making sure we are in a non secure context, as expected.
  assert_false(window.isSecureContext);
});

promise_test(async t => {
  const response = await fetch("/common/blank-with-cors.html");
  assert_true(response.ok);
}, "Local non secure context fetches local subresource.");

// This test must go through an iframe because the treat-as-public-address
// directive means that the document cannot import the test harness script, as
// that would violate the secure context restriction...
//
// For consistency and simplicity, we run all other tests in the same way, even
// though those could import the test harness from their respective origins.
promise_test(async t => {
  const url = "resources/fetcher.html" +
      "?pipe=header(Content-Security-Policy,treat-as-public-address)";
  const iframe = await appendIframe(t, document, url);

  const reply = futureMessage();
  iframe.contentWindow.postMessage("/common/blank-with-cors.html", "*");
  assert_equals(await reply, "TypeError: Failed to fetch");
}, "Treat-as-public non secure context fails to fetch local subresource.");

promise_test(async t => {
  const url = resolveUrl("resources/fetcher.html", {
    protocol: "http:",
    port: kPorts.httpPrivate,
  });
  const iframe = await appendIframe(t, document, url);

  const targetUrl = resolveUrl("/common/blank-with-cors.html");
  const reply = futureMessage();
  iframe.contentWindow.postMessage(targetUrl.href, "*");
  assert_equals(await reply, "TypeError: Failed to fetch");
}, "Private non secure context fails to fetch local subresource.");

promise_test(async t => {
  const url = resolveUrl("resources/fetcher.html", {
    protocol: "http:",
    port: kPorts.httpPublic,
  });
  const iframe = await appendIframe(t, document, url);

  const targetUrl = resolveUrl("/common/blank-with-cors.html");
  const reply = futureMessage();
  iframe.contentWindow.postMessage(targetUrl.href, "*");
  assert_equals(await reply, "TypeError: Failed to fetch");
}, "Public non secure context fails to fetch local subresource.");

promise_test(async t => {
  const url = resolveUrl("resources/fetcher.html", {
    protocol: "https:",
    port: kPorts.httpsPrivate,
  });
  const iframe = await appendIframe(t, document, url);

  const targetUrl = resolveUrl("/common/blank-with-cors.html");
  const reply = futureMessage();
  iframe.contentWindow.postMessage(targetUrl.href, "*");
  assert_equals(await reply, "TypeError: Failed to fetch");
}, "Private HTTPS yet non-secure context fails to fetch local subresource.");

promise_test(async t => {
  const url = resolveUrl("resources/fetcher.html", {
    protocol: "https:",
    port: kPorts.httpsPublic,
  });
  const iframe = await appendIframe(t, document, url);

  const targetUrl = resolveUrl("/common/blank-with-cors.html");
  const reply = futureMessage();
  iframe.contentWindow.postMessage(targetUrl.href, "*");
  assert_equals(await reply, "TypeError: Failed to fetch");
}, "Public HTTPS yet non-secure context fails to fetch local subresource.");
