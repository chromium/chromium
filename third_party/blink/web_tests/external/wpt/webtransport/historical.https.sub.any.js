// META: global=window,worker
// META: script=/common/get-host-info.sub.js
// META: script=resources/webtransport-test-helpers.sub.js

promise_test(async t => {
  // https://github.com/w3c/webtransport/commit/3e37d39bb4399935f8c88018fe3008698cad7862
  const wt = new WebTransport(webtransport_url('{{domains[nonexistent]}}'));
  // `ready` and `closed` promises will be rejected due to connection error.
  // Catches them to avoid unhandled rejections.
  wt.ready.catch(() => {});
  wt.closed.catch(() => {});
  assert_false('writable' in wt.datagrams);
}, 'WebTransportDatagramDuplexStream#writable');
