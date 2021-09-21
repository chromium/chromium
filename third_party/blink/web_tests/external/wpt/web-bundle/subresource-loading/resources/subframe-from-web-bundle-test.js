const frame_url = 'urn:uuid:429fcc4e-0696-4bad-b099-ee9175f023ae';

promise_test(async (t) => {
    const iframe = await createWebBundleElementAndIframe(t);
    // The urn:uuid URL iframe is cross-origin. So accessing
    // iframe.contentWindow.location should throws a SecurityError.
    assert_throws_dom(
      "SecurityError",
      () => { iframe.contentWindow.location.href; });
  }, 'The urn:uuid URL iframe must be cross-origin.');

urn_uuid_iframe_test(
  'location.href',
  frame_url,
  'location.href in urn uuid iframe.');

urn_uuid_iframe_test(
  '(' + (() => {
    try {
      let result = window.localStorage;
      return 'no error';
    } catch (e) {
      return e.name;
    }
  }).toString() + ')()',
  'SecurityError',
  'Accesing window.localStorage should throw a SecurityError.');

urn_uuid_iframe_test(
  '(' + (() => {
    try {
      let result = window.sessionStorage;
      return 'no error';
    } catch (e) {
      return e.name;
    }
  }).toString() + ')()',
  'SecurityError',
  'Accesing window.sessionStorage should throw a SecurityError.');

urn_uuid_iframe_test(
  '(' + (() => {
    try {
      let result = document.cookie;
      return 'no error';
    } catch (e) {
      return e.name;
    }
  }).toString() + ')()',
  'SecurityError',
  'Accesing document.cookie should throw a SecurityError.');

urn_uuid_iframe_test(
  '(' + (() => {
    try {
      let request = window.indexedDB.open("db");
      return 'no error';
    } catch (e) {
      return e.name;
    }
  }).toString() + ')()',
  'SecurityError',
  'Opening an indexedDB should throw a SecurityError.');

urn_uuid_iframe_test(
  'window.caches === undefined',
  true,
  'window.caches should be undefined.');

function urn_uuid_iframe_test(code, expected, name) {
  promise_test(async (t) => {
    const iframe = await createWebBundleElementAndIframe(t);
    assert_equals(await evalInIframe(iframe, code), expected);
  }, name);
}

async function createWebBundleElementAndIframe(t) {
  const element = createWebBundleElement(
      '../resources/wbn/urn-uuid.wbn',
      [frame_url]);
  document.body.appendChild(element);
  const iframe = document.createElement('iframe');
  t.add_cleanup(() => {
    document.body.removeChild(element);
    document.body.removeChild(iframe);
  });
  iframe.src = frame_url;
  const load_promise = new Promise((resolve) => {
    iframe.addEventListener('load', resolve);
  });
  document.body.appendChild(iframe);
  await load_promise;
  return iframe;
}

async function evalInIframe(iframe, code) {
  const message_promise = new Promise((resolve) => {
      const listener = (e) => {
        window.removeEventListener('message', listener);
        resolve(e.data);
      }
      window.addEventListener('message', listener);
    });
  iframe.contentWindow.postMessage(code,'*');
  return message_promise;
}