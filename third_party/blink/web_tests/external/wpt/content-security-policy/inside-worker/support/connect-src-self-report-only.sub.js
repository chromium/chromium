importScripts("{{location[server]}}/resources/testharness.js");
importScripts("{{location[server]}}/content-security-policy/support/testharness-helper.js");

function new_spv_promise(test, url) {
  return new Promise(resolve => {
    self.addEventListener("securitypolicyviolation", test.step_func(e => {
      if (e.blockedURI !== url)
        return;
      assert_equals(e.disposition, "report");
      resolve();
    }));
  });
}

// Same-origin
promise_test(t => {
  var url = "{{location[server]}}/content-security-policy/support/resource.py?same-origin-fetch";
  assert_no_csp_event_for_url(t, url);

  return fetch(url)
      .then(t.step_func(r => assert_equals(r.status, 200)));
}, "Same-origin 'fetch()' in " + self.location.protocol + self.location.search);

promise_test(t => {
  var url = "{{location[server]}}/content-security-policy/support/resource.py?same-origin-xhr";
  assert_no_csp_event_for_url(t, url);

  return new Promise((resolve, reject) => {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", url);
    xhr.onload = t.step_func(resolve);
    xhr.onerror = t.step_func(_ => reject("xhr.open should success."));
    xhr.send();
  });
}, "Same-origin XHR in " + self.location.protocol + self.location.search);

// Cross-origin
promise_test(t => {
  var url = "http://{{domains[www]}}:{{ports[http][1]}}/content-security-policy/support/resource.py?cross-origin-fetch";

  return Promise.all([
    new_spv_promise(t, url),
    fetch(url)
  ]);
}, "Cross-origin 'fetch()' in " + self.location.protocol + self.location.search);

promise_test(t => {
  var url = "http://{{domains[www]}}:{{ports[http][1]}}/content-security-policy/support/resource.py?cross-origin-xhr";

  return Promise.all([
    new_spv_promise(t, url),
    new Promise((resolve, reject) => {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", url);
      xhr.onload = t.step_func(resolve);
      xhr.onerror = t.step_func(_ => reject("xhr.open should not have thrown."));
      xhr.send();
    })
  ]);
}, "Cross-origin XHR in " + self.location.protocol + self.location.search);

// Same-origin redirecting to cross-origin
promise_test(t => {
  var url = "{{location[server]}}/common/redirect-opt-in.py?status=307&location=http://{{domains[www]}}:{{ports[http][1]}}/content-security-policy/support/resource.py?cross-origin-fetch";

  return Promise.all([
    new_spv_promise(t, url),
    fetch(url)
  ]);
}, "Same-origin => cross-origin 'fetch()' in " + self.location.protocol + self.location.search);

done();
