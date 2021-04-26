// META: script=constants.sub.js
// META: variant=
// META: variant=?wss
// META: variant=?wpt_flags=h2

var testOpen = async_test("Create WebSocket - set binaryType to something other than blob or arraybuffer - SYNTAX_ERR is returned - Connection should be opened");
var testClose = async_test("Create WebSocket - set binaryType to something other than blob or arraybuffer - SYNTAX_ERR is returned - Connection should be closed");

var wsocket = CreateWebSocket(false, false);

wsocket.addEventListener('open', testOpen.step_func(function(evt) {
  assert_equals(wsocket.binaryType, "blob");
  wsocket.binaryType = "notBlobOrArrayBuffer";
  assert_equals(wsocket.binaryType, "blob");
  wsocket.close();
  testOpen.done();
}), true);

wsocket.addEventListener('close', testClose.step_func(function(evt) {
  assert_true(evt.wasClean, "wasClean should be true");
  testClose.done();
}), true);
