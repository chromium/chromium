// To use the functions below, be sure to include the following files in your
// test:
// - "/common/get-host-info.sub.js" to get the different origin values.

const SAME_ORIGIN = {origin: get_host_info().HTTPS_ORIGIN, name: "SAME_ORIGIN"};
const SAME_SITE = {origin: get_host_info().HTTPS_REMOTE_ORIGIN, name: "SAME_SITE"};
const CROSS_ORIGIN = {origin: get_host_info().HTTPS_NOTSAMESITE_ORIGIN, name: "CROSS_ORIGIN"}

function addScriptAndTriggerOnload(src, onload){
  return `script = document.createElement("script");
  script.src= "${src}" ;
  script.onload = () => {
    ${onload}
  };
  document.head.append(script);`
}

function verify_window(callback, w, hasOpener) {
  // If there's no opener, the w must be closed:
  assert_equals(w.closed, !hasOpener, 'w.closed');
  // Opener's access on w.length is possible only if hasOpener:
  assert_equals(w.length, hasOpener? 1: 0, 'w.length');
  callback();
}

function validate_results(callback, test, w, channelName, hasOpener, openerDOMAccess, payload) {
  assert_equals(payload.name, hasOpener ? channelName : "", 'name');
  assert_equals(payload.opener, hasOpener, 'opener');
  // TODO(zcorpan): add openerDOMAccess expectations to all tests
  if (openerDOMAccess !== undefined) {
    assert_equals(payload.openerDOMAccess, openerDOMAccess, 'openerDOMAccess');
  }

  // The window proxy in Chromium might still reflect the previous frame,
  // until its unloaded. This delays the verification of w here.
  if( !w.closed && w.length == 0) {
    test.step_timeout( () => {
        verify_window(callback, w, hasOpener);
    }, 500);
  } else {
    verify_window(callback, w, hasOpener);
  }
}

function url_test(t, url, channelName, hasOpener, openerDOMAccess, callback) {
  if (callback === undefined) {
    callback = () => { t.done(); };
  }
  const bc = new BroadcastChannel(channelName);
  bc.onmessage = t.step_func(event => {
    const payload = event.data;
    validate_results(callback, t, w, channelName, hasOpener, openerDOMAccess, payload);
  });

  const w = window.open(url, channelName);

  // Close the popup once the test is complete.
  // The browsing context might be closed hence use the broadcast channel
  // to trigger the closure.
  t.add_cleanup(() => {
    bc.postMessage("close");
  });
}

