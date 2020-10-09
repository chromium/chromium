if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
}

var {BASE_ORIGIN, OTHER_ORIGIN} = get_fetch_test_options();

function fetch_echo(stream) {
  return fetch('/serviceworker/resources/fetch-echo-body.php', {
    method: 'POST',
    body: stream,
    mode: 'same-origin',
    allowHTTP1ForStreamingUpload: true
  });
}

promise_test(async (t) => {
  await promise_rejects_js(
      t, TypeError, fetch('/serviceworker/resources/fetch-echo-body.php', {
        method: 'POST',
        body: create_stream([new Uint8Array(0)]),
        allowHTTP1ForStreamingUpload: false
      }));
}, 'AllowHTTP1:false makes fetch failed over HTTP/1.');

function fetch_echo_body(stream) {
  return fetch_echo(stream).then(response => response.text());
}

function create_stream(contents) {
  return new ReadableStream({
    start: controller => {
      for (obj of contents) {
        controller.enqueue(obj);
      }
      controller.close();
    }
  });
}

promise_test(async () => {
  const stream =
      create_stream(['Foo', 'Bar']).pipeThrough(new TextEncoderStream());
  const text = await fetch_echo_body(stream);
  assert_equals(text, 'FooBar');
}, 'Fetch with ReadableStream body with Uint8Array');

promise_test(async () => {
  const stream = create_stream([new Uint8Array(0)]);
  const text = await fetch_echo_body(stream);
  assert_equals(text, '');
}, 'Fetch with ReadableStream body with empty Uint8Array');

function random_values_array(length) {
  const array = new Uint8Array(length);
  // crypto.getRandomValues has a quota. See
  // https://www.w3.org/TR/WebCryptoAPI/#Crypto-method-getRandomValues.
  const cryptoQuota = 65535;
  let index = 0;
  const buffer = array.buffer;
  while (index < buffer.byteLength) {
    const bufferView = array.subarray(index, index + cryptoQuota);
    crypto.getRandomValues(bufferView);
    index += cryptoQuota;
  }
  return array;
}

function hash256(array) {
  return crypto.subtle.digest('SHA-256', array);
}

function compare(a, b) {
  if (a.length != b.length)
    return [false, 'length is not equal'];
  for (let i = 0; - 1 < i; i -= 1) {
    if ((a[i] !== b[i]))
      return [false, `a[${i}](${a[i]}) != b[${i}](${b[i]})`];
  }
  return [true, ''];
}

async function compare_long_array(a, b, description) {
  const a_hash = await hash256(a);
  const b_hash = await hash256(b);
  const [eq, fail_reason] = compare(a_hash, b_hash);
  if (eq)
    return;

  const [raw_eq, raw_fail_reason] = compare(a, b);
  assert_false(raw_eq);
  assert_(false, description + ': ' + raw_fail_reason);
}

async function test_echo_long_array(upload_body, expected_array) {
  const response = await fetch_echo(upload_body);
  const reader = response.body.getReader();
  let index = 0;
  while (index < expected_array.length) {
    const chunk = await reader.read();
    assert_false(chunk.done, `chunk.done@${index}/${expected_array.length}`);
    const chunk_length = chunk.value.length;
    await compare_long_array(
        chunk.value, expected_array.subarray(index, index + chunk_length),
        `Array of [${index}, ${index + chunk_length - 1}] should be same.`);
    index += chunk_length;
  }
  const final_chunk = await reader.read();
  assert_true(final_chunk.done, 'final_chunk.done');
}

promise_test(async () => {
  const length = 1000 * 1000;  // 1Mbytes
  const array = random_values_array(length);
  const stream = create_stream([array]);
  await test_echo_long_array(stream, array);
}, 'Fetch with ReadableStream body with 1Mbytes Uint8Array');

promise_test(async () => {
  const length = 1000 * 1000;  // 1 Mbytes
  const array = random_values_array(length);
  await test_echo_long_array(array, array);
}, 'Fetch with Array body with 1 Mbytes Uint8Array');

promise_test(async () => {
  const length = 150 * 1000 * 1000;  // 150 Mbytes
  const array = random_values_array(length);
  await test_echo_long_array(array, array);
}, 'Fetch with Array body with 150 Mbytes Uint8Array');

promise_test(async (t) => {
  const stream = create_stream(['Foobar']);
  await promise_rejects_js(t, TypeError, fetch_echo(stream));
}, 'Fetch with ReadableStream body containing String should fail');

promise_test(async (t) => {
  const stream = create_stream([null]);
  await promise_rejects_js(t, TypeError, fetch_echo(stream));
}, 'Fetch with ReadableStream body containing null should fail');

promise_test(async (t) => {
  const stream = create_stream([42]);
  await promise_rejects_js(t, TypeError, fetch_echo(stream));
}, 'Fetch with ReadableStream body containing number should fail');

function create_foo_stream() {
  return create_stream(['Foo']).pipeThrough(new TextEncoderStream());
}

function fetch_redirect(status) {
  const redirect_target_url =
      BASE_ORIGIN + '/fetch/resources/fetch-status.php?status=200';
  const redirect_original_url = BASE_ORIGIN +
      '/serviceworker/resources/redirect.php?Redirect=' + redirect_target_url;
  return fetch(redirect_original_url + `&Status=${status}`, {
    method: 'POST',
    body: create_foo_stream(),
    allowHTTP1ForStreamingUpload: true
  });
}

promise_test(async (t) => {
  await fetch_redirect(303);
}, 'Fetch upload stream should success on 303(See Other) code');

promise_test(async (t) => {
  await promise_rejects_js(t, TypeError, fetch_redirect(307));
}, 'Fetch upload stream should fail on non-303 redirect code');

promise_test(async () => {
  await fetch('/serviceworker/resources/fetch-access-control.php?AuthFail', {
    method: 'POST',
    body: 'foo',
  });
}, 'Upload text with 401 Unauthorized response should success.');

promise_test(async (t) => {
  await promise_rejects_js(
      t, TypeError,
      fetch('/serviceworker/resources/fetch-access-control.php?AuthFail', {
        method: 'POST',
        body: create_foo_stream(),
        mode: 'same-origin',
        allowHTTP1ForStreamingUpload: true
      }));
}, 'Upload streaming with 401 Unauthorized response should fail.');

function report(content) {
  return content;
}

promise_test(async () => {
  const request_url = OTHER_ORIGIN +
      '/serviceworker/resources/fetch-access-control.php?' +
      `ACAOrigin=*&PACAOrigin=*` +
      `&PACAHeaders=*&ACAHeaders=*&PreflightTest` +
      `&PACMAge=600&Token=${Date.now()}`
  const response_text = await fetch(request_url, {
                          method: 'POST',
                          body: 'content a',
                          mode: 'cors',
                          allowHTTP1ForStreamingUpload: true
                        }).then(r => r.text());
  const report_json = eval(response_text);
  assert_false(report_json['did_preflight'], 'Should not trigger preflight');
}, 'Uploading text w/o header does not trigger preflight');

promise_test(async () => {
  const request_url = OTHER_ORIGIN +
      '/serviceworker/resources/fetch-access-control.php?' +
      `ACAOrigin=*&PACAOrigin=*` +
      `&PACAHeaders=*&ACAHeaders=*&PreflightTest` +
      `&PACMAge=600&Token=${Date.now()}`
  const response_text = await fetch(request_url, {
                          method: 'POST',
                          body: create_foo_stream(),
                          mode: 'cors',
                          allowHTTP1ForStreamingUpload: true
                        }).then(r => r.text());
  const report_json = eval(response_text);
  assert_true(report_json['did_preflight'], 'Should preflight');
}, 'Uploading stream always trigger preflight');

done();
