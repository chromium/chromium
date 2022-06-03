function create_window_in_test(t, srcdoc) {
  let p = new Promise((resolve) => {
    let f = document.createElement('iframe');
    f.srcdoc = srcdoc ? srcdoc : '';
    f.onload = (event) => {
      let w = f.contentWindow;
      t.add_cleanup(() => f.parentNode && f.remove());
      resolve(w);
    };
    document.body.appendChild(f);
  });
  return p;
}

function test_with_window(f, name, srcdoc) {
  promise_test((t) => {
    return create_window_in_test(t, srcdoc)
    .then((w) => {
      f(w);
    });
  }, name);
}

function assert_array_equals_callback_invocations(actual, expected, description) {
  assert_equals(actual.length, expected.length);
  for (let len=actual.length, i=0; i<len; ++i) {
    let callback = expected[i][0];
    assert_equals(actual[i][0], expected[i][0], callback + ' callback should be invoked');
    assert_equals(actual[i][1], expected[i][1], callback + ' should be invoked on the element ' + expected[i][1]);
    assert_array_equals(actual[i][2], expected[i][2], callback + ' should be invoked with the arguments ' + expected[i][2]);
  }
}

function assert_is_upgraded(element, className, description) {
  assert_true(element.matches(':defined'), description);
  assert_equals(Object.getPrototypeOf(element), className.prototype, description);
}

// Asserts that func synchronously invokes the error event handler in w.
// Captures and returns the error that is reported.
//
// Do not use this function directly; instead use one of assert_reports_js,
// assert_reports_dom, or assert_reports_exactly. Those functions also check
// that the error reported is the expected one.
function assert_reports_impl(w, func) {
  let old_onerror = w.onerror;
  let errors = [];
  w.onerror = (event, source, line_number, column_number, error) => {
    errors.push(error);
    return true; // the error is handled
  };
  try {
    func();
  } catch (e) {
    assert_unreached(`should report, not throw, an exception: ${e}`);
  } finally {
    w.onerror = old_onerror;
  }
  assert_equals(errors.length, 1, 'only one error should have been reported');
  return errors[0];
}

// Asserts that func synchronously invokes the error event handler in w
// with the expected DOMException.
function assert_reports_dom(w, expected_error, func, description) {
  const e = assert_reports_impl(w, func);
  assert_throws_dom(expected_error, w.DOMException, () => { throw e; }, description);
}

// Asserts that func synchronously invokes the error event handler in w
// with the expected JavaScript error.
function assert_reports_js(w, expected_error, func, description) {
  const e = assert_reports_impl(w, func);
  assert_throws_js(expected_error, () => { throw e; }, description);
}

// Asserts that func synchronously invokes the error event handler in w
// with exactly the expected error.
function assert_reports_exactly(w, expected_error, func, description) {
  const e = assert_reports_impl(w, func);
  assert_throws_exactly(expected_error, () => { throw e; }, description);
}
