// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/common/dispatcher/dispatcher.js
// META: script=../credentialless/resources/common.js
// META: script=./resources/common.js
// META: timeout=long

setup(() => {
  assert_implements(window.HTMLFencedFrameElement,
    "HTMLFencedFrameElement is not supported.");
})

const import_common = `
  const importScript = ${importScript};
  await importScript("/common/utils.js");
  await importScript("/html/cross-origin-embedder-policy/credentialless" +
    "/resources/common.js");
  await importScript("/html/cross-origin-embedder-policy/anonymous-iframe" +
    "/resources/common.js");
`;

// 3 actors:
// - A popup, enforcing COEP.
// - An AnonymousFrame, omitting COEP, inside the popup.
// - A FencedFrame, inside the anonymous frame.
//
// This answers the question: Does the FencedFrame require COEP/CORP?
promise_test_parallel(async test => {
  const same_origin = get_host_info().HTTPS_ORIGIN;
  const cross_origin = get_host_info().HTTPS_REMOTE_ORIGIN;
  const msg_queue = token();

  // 1. Create a popup, enforcing COEP:
  const popup = newPopup(same_origin, coep_require_corp);

  // 2. Inside, create an AnonymousFrame.
  send(popup, `
    ${import_common}
    const iframe_anonymous = newAnonymousIframe("${same_origin}");
    send("${msg_queue}", iframe_anonymous);
  `);
  const iframe_anonymous = await receive(msg_queue);
  send(iframe_anonymous, `
    ${import_common}
    send("${msg_queue}", "Script imported");
  `);
  assert_equals(await receive(msg_queue), "Script imported");

  // 3. Inside, create a FencedFrame. Does it load?
  // Several variations depending on its origin and headers.
  const test_cases = [
    {
      description: "same-origin => blocked.",
      origin: same_origin, headers: "",
      expectation: "FencedFrame blocked",
    }, {
      description: "cross-origin => blocked.",
      origin: cross_origin, headers: "",
      expectation: "FencedFrame blocked",
    }, {
      description: "same-origin + coep => blocked.",
      origin: same_origin, headers: coep_require_corp,
      expectation: "FencedFrame blocked",
    }, {
      description: "cross-origin + coep => blocked.",
      origin: cross_origin, headers: coep_require_corp,
      expectation: "FencedFrame blocked",
    }, {
      description: "same-origin + coep + corp => allowed.",
      origin: same_origin, headers: coep_require_corp + corp_cross_origin,
      expectation: "FencedFrame loaded",
    }, {
      description: "cross-origin + coep + corp => allowed.",
      origin: cross_origin, headers: coep_require_corp + corp_cross_origin,
      expectation: "FencedFrame loaded",
    },
  ];

  for(const test_case of test_cases) {
    promise_test_parallel(async test => {
      const msg_queue = token();
      send(iframe_anonymous, `
        const iframe_fenced = newFencedFrame("${test_case.origin}",
                                             "${test_case.headers}");
        send("${msg_queue}", iframe_fenced);
      `);
      const iframe_fenced = await receive(msg_queue);

      // Test depends on what comes first, a reply from the FencedFrame or a
      // timeout.
      send(iframe_fenced, `
        send("${msg_queue}", "FencedFrame loaded")
      `);
      test.step_timeout(() => {
        send(msg_queue, "FencedFrame blocked")
      }, 5000);

      assert_equals(await receive(msg_queue), test_case.expectation);
    }, test_case.description);
  }

}, "Check FencedFrame check adherence to COEP within an anonymous frame");
