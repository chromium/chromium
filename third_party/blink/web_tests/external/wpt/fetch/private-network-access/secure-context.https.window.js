// META: script=resources/support.js
// META: script=resources/ports.sub.js
// META: script=resources/resolve_url.js
//
// Spec: https://wicg.github.io/private-network-access/#integration-fetch
//
// This file covers only those tests that must execute in a secure context.
// Other tests are defined in: non-secure-context.window.js

setup(() => {
  // Making sure we are in a secure context, as expected.
  assert_true(window.isSecureContext);
});

promise_test(async t => {
  const response = await fetch("/common/blank.html")
  assert_true(response.ok);
}, "Local secure context fetches local subresource.");

// For the following tests, we go through an iframe, because it is not possible
// to directly import the test harness from a secured public page.
promise_test(async t => {
  const url = "resources/fetcher.html" +
      "?pipe=header(Content-Security-Policy,treat-as-public-address)";
  const iframe = await appendIframe(t, document, url);

  const reply = futureMessage();
  iframe.contentWindow.postMessage("/common/blank-with-cors.html", "*");
  assert_equals(await reply, true);
}, "Treat-as-public secure context fetches local subresource.");

promise_test(async t => {
  const url = resolveUrl("resources/fetcher.html", {
    protocol: "https:",
    port: kPorts.httpsPrivate,
  });
  const iframe = await appendIframe(t, document, url);

  const targetUrl = resolveUrl("/common/blank-with-cors.html");
  const reply = futureMessage();
  iframe.contentWindow.postMessage(targetUrl.href, "*");
  assert_equals(await reply, true);
}, "Private secure context can fetch local subresource.");

promise_test(async t => {
  const url = resolveUrl("resources/fetcher.html", {
    protocol: "https:",
    port: kPorts.httpsPublic,
  });
  const iframe = await appendIframe(t, document, url);

  const targetUrl = resolveUrl("/common/blank-with-cors.html");
  const reply = futureMessage();
  iframe.contentWindow.postMessage(targetUrl.href, "*");
  assert_equals(await reply, true);
}, "Public secure context can fetch local subresource.");
