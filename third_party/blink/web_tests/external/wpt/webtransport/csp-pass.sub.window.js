// META: global=window,worker
// META: script=/common/get-host-info.sub.js

const HOST = get_host_info().ORIGINAL_HOST;
const PORT = '{{ports[webtransport-h3][0]}}';
const BASE = `https://${HOST}:${PORT}`;

function set_csp(destination) {
  let meta = document.createElement("meta");
  meta.httpEquiv = "Content-Security-Policy";
  meta.content = `connect-src ${destination}`;
  return meta;
}

promise_test(async t => {
 let meta = set_csp(`${BASE}`);
 document.head.appendChild(meta);

  let wt = new WebTransport(`${BASE}/webtransport/handlers/custom-response.py?:status=200`);
  await wt.ready;
}, "WebTransport connection should succeed when CSP connect-src destination is set to the page");
