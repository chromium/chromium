// META: script=constants.sub.js
// META: variant=
// META: variant=?wss
// META: variant=?wpt_flags=h2

var testOpen = async_test("Create WebSocket - Close the Connection - close() - Connection should be opened");
var testClose = async_test("Create WebSocket - Close the Connection - close() - return close code is 1005 - Connection should be closed");

var wsocket = CreateWebSocket(false, false);
var isOpenCalled = false;

wsocket.addEventListener('open', testOpen.step_func(function(evt) {
  wsocket.close();
  isOpenCalled = true;
  testOpen.done();
}), true);

wsocket.addEventListener('close', testClose.step_func(function(evt) {
  assert_true(isOpenCalled, "WebSocket connection should be open");
  assert_equals(evt.code, 1005, "CloseEvent.code should be 1005");
  assert_equals(evt.reason, "", "CloseEvent.reason should be empty");
  testClose.done();
}), true);
