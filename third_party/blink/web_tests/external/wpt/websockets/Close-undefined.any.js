// META: script=constants.sub.js
// META: variant=
// META: variant=?wpt_flags=h2
// META: variant=?wss

var test = async_test();

var wsocket = CreateWebSocket(false, false);
var isOpenCalled = false;

wsocket.addEventListener('open', test.step_func(function(evt) {
  wsocket.close(undefined);
  test.done();
}), true);
