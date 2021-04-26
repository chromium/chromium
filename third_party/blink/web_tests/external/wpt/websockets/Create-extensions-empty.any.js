// META: timeout=long
// META: script=constants.sub.js
// META: variant=
// META: variant=?wpt_flags=h2
// META: variant=?wss

var testOpen = async_test("Create WebSocket - wsocket.extensions should be set to '' after connection is established - Connection should be opened");
var testClose = async_test("Create WebSocket - wsocket.extensions should be set to '' after connection is established - Connection should be closed");

var wsocket = new WebSocket(SCHEME_DOMAIN_PORT + "/handshake_no_extensions");
var isOpenCalled = false;

wsocket.addEventListener('open', testOpen.step_func_done(function(evt) {
  wsocket.close();
  isOpenCalled = true;
  assert_equals(wsocket.extensions, "", "extensions should be empty");
}), true);

wsocket.addEventListener('close', testClose.step_func_done(function(evt) {
  assert_true(isOpenCalled, "WebSocket connection should be closed");
}), true);
