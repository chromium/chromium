// Verifies that non-local HTTP(S) ports are open and serve correctly.
//
// See the corresponding WPT RFC:
// https://github.com/web-platform-tests/rfcs/blob/master/rfcs/address_space_overrides.md
//
// These ports are used to test the Private Network Access specification:
// https://wicg.github.io/private-network-access/
//
// More tests can be found in `fetch/private-network-access/`.

// Resolves a URL relative to the current location, returning an absolute URL.
//
// `url` specifies the relative URL, e.g. "foo.html" or "http://foo.example".
// `options.protocol` and `options.port`, if defined, override the respective
// properties of the returned URL object.
function resolveUrl(url, options) {
  const result = new URL(url, window.location);
  if (options === undefined) {
    return result;
  }

  const { port, protocol } = options;
  if (port !== undefined) {
    result.port = port;
  }
  if (protocol !== undefined) {
    result.protocol = protocol;
  }

  return result;
}

promise_test(async () => {
  const url = resolveUrl("/common/blank-with-cors.html", {
    protocol: "https:",
    port: "{{ports[https-private][0]}}"
  });
  const response = await fetch(url);
  assert_true(response.ok);
}, "Fetch from https-private port works.");

promise_test(async () => {
  const url = resolveUrl("/common/blank-with-cors.html", {
    protocol: "http:",
    port: "{{ports[http-private][0]}}"
  });
  const response = await fetch(url);
  assert_true(response.ok);
}, "Fetch from http-private port works.");

promise_test(async () => {
  const url = resolveUrl("/common/blank-with-cors.html", {
    protocol: "https:",
    port: "{{ports[https-public][0]}}"
  });
  const response = await fetch(url);
  assert_true(response.ok);
}, "Fetch from https-public port works.");

promise_test(async () => {
  const url = resolveUrl("/common/blank-with-cors.html", {
    protocol: "http:",
    port: "{{ports[http-public][0]}}"
  });
  const response = await fetch(url);
  assert_true(response.ok);
}, "Fetch from http-public port works.");
