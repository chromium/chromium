// META: script=/common/utils.js
// META: script=resources/support.sub.js
//
// Spec: https://wicg.github.io/private-network-access/#integration-fetch
//
// These tests check that initial `ServiceWorker` script fetches are subject to
// Private Network Access checks, just like a regular `fetch()`.
// See also: worker.https.window.js

const TestResult = {
  SUCCESS: {
    loaded: true,
    unregistered: true,
  },
  FAILURE: {
    loaded: false,
    error: "TypeError",
  },
};

async function serviceWorkerScriptTest(t, { source, target, expected }) {
  const sourceUrl = resolveUrl("resources/service-worker-fetcher.html",
                               sourceResolveOptions(source));

  const targetUrl =
      resolveUrl("resources/preflight.py", targetResolveOptions(target));
  targetUrl.searchParams.append("body", "undefined");
  targetUrl.searchParams.append("mime-type", "application/javascript");

  const iframe = await appendIframe(t, document, sourceUrl);
  const reply = futureMessage();

  const message = {
    url: targetUrl.href,
    uuid: token(),
  };
  iframe.contentWindow.postMessage(message, "*");

  const { error, loaded, unregistered } = await reply;

  assert_equals(error, expected.error, "error");
  assert_equals(loaded, expected.loaded, "response loaded");
  assert_equals(unregistered, expected.unregistered, "worker unregistered");
}

promise_test(t => serviceWorkerScriptTest(t, {
  source: {
    server: Server.HTTPS_LOCAL,
    treatAsPublic: true,
  },
  target: { server: Server.HTTPS_LOCAL },
  expected: TestResult.FAILURE,
}), "treat-as-public to local: failed preflight.");

promise_test(t => serviceWorkerScriptTest(t, {
  source: {
    server: Server.HTTPS_LOCAL,
    treatAsPublic: true,
  },
  target: {
    server: Server.HTTPS_LOCAL,
    behavior: { preflight: PreflightBehavior.success(token()) },
  },
  expected: TestResult.SUCCESS,
}), "treat-as-public to local: success.");

promise_test(t => serviceWorkerScriptTest(t, {
  source: {
    server: Server.HTTPS_PRIVATE,
    treatAsPublic: true,
  },
  target: { server: Server.HTTPS_PRIVATE },
  expected: TestResult.FAILURE,
}), "treat-as-public to private: failed preflight.");

promise_test(t => serviceWorkerScriptTest(t, {
  source: {
    server: Server.HTTPS_PRIVATE,
    treatAsPublic: true,
  },
  target: {
    server: Server.HTTPS_PRIVATE,
    behavior: { preflight: PreflightBehavior.success(token()) },
  },
  expected: TestResult.SUCCESS,
}), "treat-as-public to private: success.");

promise_test(t => serviceWorkerScriptTest(t, {
  source: { server: Server.HTTPS_PUBLIC },
  target: { server: Server.HTTPS_PUBLIC },
  expected: TestResult.SUCCESS,
}), "public to public: success.");
