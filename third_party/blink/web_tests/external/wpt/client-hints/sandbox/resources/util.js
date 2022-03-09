const iframe_src =
  "/client-hints/resources/expect-client-hints-headers-iframe.py";

const expect_iframe_no_hints = iframe_src +
  "?sec-ch-device-memory=false" +
  "&device-memory=false" +
  "&sec-ch-dpr=false" +
  "&dpr=false" +
  "&sec-ch-viewport-width=false" +
  "&viewport-width=false" +
  "&sec-ch-ua=true" +
  "&sec-ch-ua-mobile=true";

const expect_iframe_hints = iframe_src +
  "?sec-ch-device-memory=true" +
  "&device-memory=true" +
  "&sec-ch-dpr=true" +
  "&dpr=true" +
  "&sec-ch-viewport-width=true" +
  "&viewport-width=true" +
  "&sec-ch-ua=true" +
  "&sec-ch-ua-mobile=true";

function sandbox_iframe_test(sandbox, src, title) {
  return promise_test(async t => {
    const iframe = document.createElement("iframe");
    if (sandbox !== "")
      iframe.sandbox = sandbox;
    iframe.src = src;

    let msg = await new Promise(resolve => {
      window.addEventListener('message', resolve);
      document.body.appendChild(iframe);
    });

    assert_equals(msg.data, "PASS", "message from opened frame");
    await fetch("/client-hints/accept-ch-stickiness/resources/clear-site-data.html");
  }, title);
}

function sandbox_popup_listener(src) {
  window.addEventListener('message', e => {
    window.parent.postMessage(e.data, '*');
  });

  let popup_window = window.open("/resources/blank.html");
  popup_window.location.href = src;
}