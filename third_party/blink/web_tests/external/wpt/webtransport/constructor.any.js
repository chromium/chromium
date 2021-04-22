// META: global=window,worker
// META: script=/common/get-host-info.sub.js

const HOST = get_host_info().ORIGINAL_HOST;

const BAD_URLS = [
  null,
  '',
  'no-scheme',
  'http://example.com/' /* scheme is wrong */,
  'quic-transport://example.com/' /* scheme is wrong */,
  'https:///' /* no host  specified */,
  'https://example.com/#failing' /* has fragment */,
  `https://${HOST}:999999/` /* invalid port */,
];

for (const url of BAD_URLS) {
  test(() => {
    assert_throws_dom('SyntaxError', () => new WebTransport(url),
                      'constructor should throw');
  }, `WebTransport constructor should reject URL '${url}'`);
}

// TODO(bashi): Test CSP.

promise_test(t => {
  const wt = new WebTransport(`https://${HOST}:0/`);
  return Promise.all([
    promise_rejects_js(t, TypeError, wt.ready, 'ready promise rejects'),
    promise_rejects_js(t, TypeError, wt.closed, 'closed promise rejects'),
  ]);
}, 'connection to port 0 should fail');
