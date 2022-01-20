// META: timeout=long
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/common/dispatcher/dispatcher.js
// META: script=../credentialless/resources/common.js
// META: script=./resources/common.js

// A script listening using a BroadcastChannel.
const listen_script = (key, done, onmessage) => `
  const bc = new BroadcastChannel("${key}");
  bc.onmessage = event => send("${onmessage}", event.data);
  send("${done}", "registered");
`;

const emit_script = (key, message) => `
  const bc = new BroadcastChannel("${key}");
  bc.postMessage("${message}");
`;

// When using BroadcastChannel with Firefox, in [5%,10%] of the runs, it does
// not complete. It ends up with a timeout instead of a failure. It doesn't
// complete, even when reaching the end of every tests. For instance, there was
// previously an error triggered for every tests after 9 seconds, guaranteeing
// every tests to complete with an error at worst. Even with this, it was still
// failing with a timeout. This suggests a problem in the test runner and maybe
// how window contexts are released when BroadcastChannel is used. Maybe there
// are some code to delay their deletion to ensure pending messages are sent?
//
// TODO(arthursonzogni@chromium.org) Investigate and re-enable this test.
setup(() => {
  assert_true(navigator.userAgent.search("Firefox") == -1,
    "Disabled for Firefox, because it fails in a flaky manner");
});

promise_test_parallel(async test => {
  const origin = get_host_info().HTTPS_REMOTE_ORIGIN;
  const key_1 = token();
  const key_2 = token();

  // 2 actors: An anonymous iframe and a normal one.
  const iframe_anonymous = newAnonymousIframe(origin);
  const iframe_normal = newIframe(origin);
  const queue_1 = token();
  const queue_2 = token();
  const unexpected_queue = token();

  // Listen using the two keys from both sides:
  send(iframe_anonymous , listen_script(key_1, queue_1, queue_1));
  send(iframe_anonymous , listen_script(key_2, queue_1, unexpected_queue));
  send(iframe_normal, listen_script(key_2, queue_2, queue_2));
  send(iframe_normal, listen_script(key_1, queue_2, unexpected_queue));
  assert_equals(await receive(queue_1), "registered");
  assert_equals(await receive(queue_1), "registered");
  assert_equals(await receive(queue_2), "registered");
  assert_equals(await receive(queue_2), "registered");

  // Emit from both sides. It must work, and work without crossing the
  // anonymous/non-anonymous border.
  receive(unexpected_queue).then(test.unreached_func(
    "BroadcastChannel shouldn't cross the anonymous/normal border"));
  send(iframe_anonymous , emit_script(key_1, "msg_1"));
  send(iframe_normal, emit_script(key_2, "msg_2"));
  assert_equals(await receive(queue_1), "msg_1");
  assert_equals(await receive(queue_2), "msg_2");

  // Wait a bit to let bad things the opportunity to show up. This is done by
  // repeating the previous operation.
  send(iframe_anonymous , emit_script(key_1, "msg_3"));
  send(iframe_normal, emit_script(key_2, "msg_4"));
  assert_equals(await receive(queue_1), "msg_3");
  assert_equals(await receive(queue_2), "msg_4");
}, "Anonymous iframe and normal iframe aren't in the same partition")

promise_test_parallel(async test => {
  const origin = get_host_info().HTTPS_REMOTE_ORIGIN;
  const key = token();

  const iframe_anonymous_1 = newAnonymousIframe(origin);
  const iframe_anonymous_2 = newAnonymousIframe(origin);
  const queue = token();

  send(iframe_anonymous_1 , listen_script(key, queue, queue));
  assert_equals(await receive(queue), "registered");
  send(iframe_anonymous_2, emit_script(key, "msg"));
  assert_equals(await receive(queue), "msg");
}, "Two sibling same-origin anonymous iframes are in the same partition");

promise_test_parallel(async test => {
  const origin = get_host_info().HTTPS_REMOTE_ORIGIN;
  const key = token();
  const queue = token();

  const iframe_anonymous_1 = newAnonymousIframe(origin);
  send(iframe_anonymous_1, `
    const importScript = ${importScript};
    await importScript("/common/utils.js");
    await importScript("/html/cross-origin-embedder-policy/credentialless" +
                       "/resources/common.js");
    const newAnonymousIframe = ${newAnonymousIframe};
    const iframe_anonymous_2 = newAnonymousIframe("${origin}");
    send("${queue}", iframe_anonymous_2);
  `);
  const iframe_anonymous_2 = await receive(queue);

  send(iframe_anonymous_1 , listen_script(key, queue, queue));
  assert_equals(await receive(queue), "registered");
  send(iframe_anonymous_2, emit_script(key, "msg"));
  assert_equals(await receive(queue), "msg");
}, "Nested same-origin anonymous iframe are in the same partition");

promise_test_parallel(async test => {
  const origin = get_host_info().HTTPS_REMOTE_ORIGIN;
  const key = token();
  const queue = token();

  const iframe_anonymous_1 = newAnonymousIframe(origin);
  const popup = newPopup(test, origin);
  send(popup, `
    const importScript = ${importScript};
    await importScript("/common/utils.js");
    await importScript("/html/cross-origin-embedder-policy/credentialless" +
                       "/resources/common.js");
    const newAnonymousIframe = ${newAnonymousIframe};
    send("${queue}", newAnonymousIframe("${origin}"));
  `);
  const iframe_anonymous_2 = await receive(queue);

  const unexpected_queue = token();
  receive(unexpected_queue).then(test.unreached_func(
    "Two same-origin anonymous iframe in different windows shouldn't be able " +
    "to communicate using BroadcastChannel"));

  send(iframe_anonymous_1 , listen_script(key, queue, unexpected_queue));
  assert_equals(await receive(queue), "registered");
  await send(iframe_anonymous_2, emit_script(key, "msg"));

  // Wait a bit to give the opportunity for unexpected message to be received.
  await new Promise(r => test.step_timeout(r, 500));
}, "Two anonymous iframes in different windows do not share the same " +
   "partition");
