if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
}

promise_test(function(t) {
    return fetch('http://')
      .then(
        t.unreached_func('fetch of invalid URL must fail'),
        function() {});
  }, 'Fetch invalid URL');

// https://fetch.spec.whatwg.org/#fetching
// Step 4:
// request's url's scheme is not one of "http" and "https"
//   A network error.
promise_test(function(t) {
    return fetch('ftp://localhost/')
      .then(
        t.unreached_func('fetch of non-HTTP(S) CORS must fail'),
        function() {});
  }, 'fetch non-HTTP(S) CORS');

// Tests for data: scheme.
promise_test(function(t) {
    return fetch('data:,Foobar')
      .then(function(response) {
          assert_equals(response.status, 200);
          assert_equals(response.statusText, 'OK');
          assert_equals(response.headers.get('Content-Type'),
                        'text/plain;charset=US-ASCII');
          assert_equals(size(response.headers), 1);
          assert_false(response.redirected);
          if (self.internals) {
            assert_array_equals(
                self.internals.getInternalResponseURLList(response),
                ['data:,Foobar']);
          }
          return response.text();
        })
      .then(function(text) {
          assert_equals(text, 'Foobar');
        });
  }, 'fetch data: URL');

promise_test(function(t) {
    return fetch('data:,Foobar',
                 {
                   method: 'POST',
                   body: 'Test'
                 })
      .then(function(response) {
          assert_equals(response.status, 200);
          assert_equals(response.statusText, 'OK');
          assert_equals(response.headers.get('Content-Type'),
                        'text/plain;charset=US-ASCII');
          assert_equals(size(response.headers), 1);
          return response.text();
        })
      .then(function(text) {
          assert_equals(text, 'Foobar');
        });
  }, 'fetch data: URL with the POST method');

promise_test(function(t) {
    return fetch('data:text/html;charset=utf-8;base64,5paH5a2X')
      .then(function(response) {
          assert_equals(response.status, 200);
          assert_equals(response.statusText, 'OK');
          assert_equals(response.headers.get('Content-Type'),
                        'text/html;charset=utf-8');
          assert_equals(size(response.headers), 1);
          assert_false(response.redirected);
          if (self.internals) {
            assert_array_equals(
                self.internals.getInternalResponseURLList(response),
                ['data:text/html;charset=utf-8;base64,5paH5a2X']);
          }
          return response.text();
        })
      .then(function(text) {
          assert_equals(text, '\u6587\u5b57');
        });
  }, 'fetch data: URL with non-ASCII characters');

promise_test(function(t) {
    return fetch('data:text/html;base64,***')
      .then(
        t.unreached_func('fetching invalid data: URL must fail'),
        function() {});
  }, 'fetch invalid data: URL');

promise_test(function(t) {
    return fetch('data:,Foobar',
                 {
                   method: 'HEAD'
                 })
      .then(function(response) {
          assert_equals(response.status, 200);
          assert_equals(response.statusText, 'OK');
          assert_equals(response.headers.get('Content-Type'),
                        'text/plain;charset=US-ASCII');
          assert_equals(size(response.headers), 1);
          assert_equals(response.type, 'basic', 'type must match.');
          return response.text();
        })
      .then(function(text) {
          assert_equals(text, '');
        });
  }, 'fetch data: URL with the HEAD method');

// Only [Exposed=(Window,DedicatedWorker,SharedWorker)].
if ('createObjectURL' in URL) {
  // Tests for blob: scheme.
  promise_test(function(t) {
      var url = URL.createObjectURL(new Blob(['fox'], {type: 'text/fox'}));
      return fetch(url)
        .then(function(response) {
            assert_equals(response.status, 200);
            assert_equals(response.statusText, 'OK');
            assert_equals(response.headers.get('Content-Type'), 'text/fox');
            assert_equals(response.headers.get('Content-Length'), '3');
            assert_equals(size(response.headers), 2);
            assert_false(response.redirected);
            if (self.internals) {
              assert_array_equals(
                  self.internals.getInternalResponseURLList(response), [url]);
            }
            return response.text();
          })
        .then(function(text) {
            assert_equals(text, 'fox');
          });
    }, 'fetch blob: URL');

  promise_test(function(t) {
      var url = URL.createObjectURL(new Blob(['fox'], {type: 'text/fox'}));
      return fetch(url + 'invalid')
        .then(
          t.unreached_func('fetching non-existent blob: URL must fail'),
          function() {});
    }, 'fetch non-existent blob: URL');
}

// https://fetch.spec.whatwg.org/#concept-basic-fetch
// The last statement:
// Otherwise
//   Return a network error.
promise_test(function(t) {
    return fetch('foobar://localhost/', {mode: 'no-cors'})
      .then(
        t.unreached_func('scheme not listed in basic fetch spec must fail'),
        function() {});
  }, 'fetch of scheme not listed in basic fetch spec');

promise_test(function(t) {
    var request = new Request('/fetch/resources/fetch-status.php?status=200');
    return fetch(request)
      .then(function(response) {
          assert_equals(response.status, 200);
          assert_equals(response.statusText, 'OK');
          assert_false(response.redirected);
          if (self.internals) {
            assert_array_equals(
                self.internals.getInternalResponseURLList(response),
                [request.url]);
          }
        });
  }, 'Fetch result of 200 response');

promise_test(function(t) {
    var request = new Request('/fetch/resources/fetch-status.php?status=404');
    return fetch(request)
      .then(function(response) {
          assert_equals(response.status, 404);
          assert_equals(response.statusText, 'Not Found');
          assert_false(response.redirected);
          if (self.internals) {
            assert_array_equals(
                self.internals.getInternalResponseURLList(response),
                [request.url]);
          }
        });
  }, 'Fetch result of 404 response');

promise_test(function(t) {
    var request = new Request(
      '/fetch/resources/fetch-status.php?status=200#fragment');
    assert_equals(request.url,
      BASE_ORIGIN + '/fetch/resources/fetch-status.php?status=200#fragment');

    return fetch(request)
      .then(function(response) {
          assert_equals(response.status, 200);
          assert_equals(response.statusText, 'OK');
          // The url attribute's getter must return the empty string
          // if response's url is null and response's url,
          // serialized with the exclude fragment flag set, otherwise.
          assert_equals(response.url,
            BASE_ORIGIN + '/fetch/resources/fetch-status.php?status=200');
          assert_false(response.redirected);
          if (self.internals) {
            assert_array_equals(
                self.internals.getInternalResponseURLList(response),
                [BASE_ORIGIN +
                 '/fetch/resources/fetch-status.php?status=200#fragment']);
          }
        });
  }, 'Request/response url attribute getter with fragment');

promise_test(function(t) {
    var redirect_target_url =
      BASE_ORIGIN + '/fetch/resources/fetch-status.php?status=200';
    var redirect_original_url =
      BASE_ORIGIN + '/serviceworker/resources/redirect.php?Redirect=' +
      redirect_target_url;

    var request = new Request(redirect_original_url);
    assert_equals(request.url, redirect_original_url,
      'Request\'s url is the original URL');
    assert_equals(request.redirect, 'follow');

    return fetch(request)
      .then(function(response) {
          assert_equals(response.status, 200);
          assert_equals(response.statusText, 'OK');
          assert_equals(response.url, redirect_target_url,
            'Response\'s url is locationURL');
          assert_equals(request.url, redirect_original_url,
            'Request\'s url remains the original URL');
          assert_true(response.redirected);
          if (self.internals) {
            assert_array_equals(
                self.internals.getInternalResponseURLList(response),
                [request.url, response.url]);
          }
        });
  }, 'Request/response url attribute getter with redirect');

promise_test(function(t) {
    var redirect_target_url =
      BASE_ORIGIN + '/fetch/resources/fetch-status.php?status=200';
    var redirect_original_url =
      BASE_ORIGIN + '/serviceworker/resources/redirect.php?Redirect=' +
      redirect_target_url;

    var request = new Request(redirect_original_url, {redirect: 'manual'});
    assert_equals(request.url, redirect_original_url,
      'Request\'s url is the original URL');
    assert_equals(request.redirect, 'manual');

    return fetch(request)
      .then(function(response) {
          assert_equals(response.status, 0);
          assert_equals(response.type, 'opaqueredirect');
          assert_equals(response.url, request.url);
          assert_false(response.redirected);
          if (self.internals) {
            assert_array_equals(
                self.internals.getInternalResponseURLList(response),
                [redirect_original_url]);
          }
        });
  }, 'Manual redirect fetch returns opaque redirect response');

promise_test(function(t) {
    var redirect_original_url =
      BASE_ORIGIN + '/serviceworker/resources/redirect.php?Redirect=noLocation';

    var request = new Request(redirect_original_url, {redirect: 'manual'});
    assert_equals(request.url, redirect_original_url,
      'Request\'s url is the original URL');
    assert_equals(request.redirect, 'manual');

    return fetch(request)
      .then(function(response) {
          assert_equals(response.status, 0);
          assert_equals(response.type, 'opaqueredirect');
          assert_equals(response.url, request.url);
          assert_false(response.redirected);
          if (self.internals) {
            assert_array_equals(
                self.internals.getInternalResponseURLList(response),
                [redirect_original_url]);
          }
        });
  }, 'Manual redirect fetch returns opaque redirect response even if location' +
     'header is not set');

promise_test(function(t) {
    var redirect_target_url =
      OTHER_ORIGIN + '/fetch/resources/fetch-status.php?status=200';
    var redirect_original_url =
      OTHER_ORIGIN + '/serviceworker/resources/redirect.php?Redirect=' +
      redirect_target_url;

    var request = new Request(redirect_original_url,
                              {headers: [['X-Fetch-Test', 'A']],
                               redirect: 'manual'});

    // Cross-origin request with non-simple header initiates CORS preflight
    // request.
    return fetch(request)
      .then(
        t.unreached_func('Even in manual redirect mode, fetch with preflight' +
                         ' must fail when redirect response is received'),
        function() {});
  }, 'Even in manual redirect mode, fetch with preflight must fail when' +
     ' redirect response is received');

promise_test(function(t) {
    var redirect_target_url =
      BASE_ORIGIN + '/fetch/resources/fetch-status.php?status=200';
    var redirect_original_url =
      BASE_ORIGIN + '/serviceworker/resources/redirect.php?Redirect=' +
      redirect_target_url;

    var request = new Request(redirect_original_url, {redirect: 'error'});
    assert_equals(request.url, redirect_original_url,
      'Request\'s url is the original URL');
    assert_equals(request.redirect, 'error');

    return fetch(request)
      .then(
        t.unreached_func('Redirect response must cause an error when redirect' +
                         ' mode is error.'),
        function() {});
  }, 'Redirect response must cause an error when redirect mode is error.');

promise_test(function(test) {
    var url = BASE_ORIGIN + '/fetch/resources/doctype.html';
    return fetch(new Request(url, {redirect: 'manual'}))
      .then(function(response) {
          assert_equals(response.status, 200);
          assert_equals(response.statusText, 'OK');
          assert_equals(response.url, url);
          assert_false(response.redirected);
          if (self.internals) {
            assert_array_equals(
                self.internals.getInternalResponseURLList(response), [url]);
          }
          return response.text();
        })
      .then(function(text) { assert_equals(text, '<!DOCTYPE html>\n'); })
    }, 'No-redirect fetch completes normally even if redirect mode is manual');

promise_test(function(test) {
    var url = BASE_ORIGIN + '/fetch/resources/doctype.html';
    return fetch(new Request(url, {redirect: 'error'}))
      .then(function(response) {
          assert_equals(response.status, 200);
          assert_equals(response.statusText, 'OK');
          assert_equals(response.url, url);
          assert_false(response.redirected);
          if (self.internals) {
            assert_array_equals(
                self.internals.getInternalResponseURLList(response), [url]);
          }
          return response.text();
        })
      .then(function(text) { assert_equals(text, '<!DOCTYPE html>\n'); })
    }, 'No-redirect fetch completes normally even if redirect mode is error');

function evalJsonp(text) {
  return new Promise(function(resolve) {
      var report = resolve;
      // text must contain report() call.
      eval(text);
    });
}

promise_test(function(t) {
    var request =
      new Request('/serviceworker/resources/fetch-access-control.php',
                  {
                    method: 'POST',
                    body: new Blob(['Test Blob'], {type: 'test/type'})
                  });
    return fetch(request)
      .then(function(response) { return response.text(); })
      .then(evalJsonp)
      .then(function(result) {
          assert_equals(result.method, 'POST');
          assert_equals(result.body, 'Test Blob');
        });
  }, 'Fetch with Blob body test');

promise_test(function(t) {
    var request = new Request(
      '/serviceworker/resources/fetch-access-control.php',
      {method: 'POST', body: 'Test String'});
    return fetch(request)
      .then(function(response) { return response.text(); })
      .then(evalJsonp)
      .then(function(result) {
          assert_equals(result.method, 'POST');
          assert_equals(result.body, 'Test String');
        });
  }, 'Fetch with string body test');

promise_test(function(t) {
    var text = 'Test ArrayBuffer';
    var array = new Uint8Array(text.length);
    for (var i = 0; i < text.length; ++i)
      array[i] = text.charCodeAt(i);
    var request = new Request(
      '/serviceworker/resources/fetch-access-control.php',
      {method: 'POST', body: array.buffer});
    return fetch(request)
      .then(function(response) { return response.text(); })
      .then(evalJsonp)
      .then(function(result) {
          assert_equals(result.method, 'POST');
          assert_equals(result.body, 'Test ArrayBuffer');
        });
  }, 'Fetch with ArrayBuffer body test');

promise_test(function(t) {
    var text = 'Test ArrayBufferView';
    var array = new Uint8Array(text.length);
    for (var i = 0; i < text.length; ++i)
      array[i] = text.charCodeAt(i);
    var request = new Request(
      '/serviceworker/resources/fetch-access-control.php',
      {method: 'POST', body: array});
    return fetch(request)
      .then(function(response) { return response.text(); })
      .then(evalJsonp)
      .then(function(result) {
          assert_equals(result.method, 'POST');
          assert_equals(result.body, 'Test ArrayBufferView');
        });
  }, 'Fetch with ArrayBufferView body test');

promise_test(function(t) {
    var formData = new FormData();
    formData.append('StringKey1', '1234567890');
    formData.append('StringKey2', 'ABCDEFGHIJ');
    formData.append('BlobKey', new Blob(['blob content']));
    formData.append('FileKey',
                    new File(['file content'], 'file.dat'));
    var request = new Request(
      '/serviceworker/resources/fetch-access-control.php',
      {method: 'POST', body: formData});
    return fetch(request)
      .then(function(response) { return response.text(); })
      .then(evalJsonp)
      .then(function(result) {
          assert_equals(result.method, 'POST');
          assert_equals(result.post['StringKey1'], '1234567890');
          assert_equals(result.post['StringKey2'], 'ABCDEFGHIJ');
          var files = [];
          for (var i = 0; i < result.files.length; ++i) {
            files[result.files[i].key] = result.files[i];
          }
          assert_equals(files['BlobKey'].content, 'blob content');
          assert_equals(files['BlobKey'].name, 'blob');
          assert_equals(files['BlobKey'].size, 12);
          assert_equals(files['FileKey'].content, 'file content');
          assert_equals(files['FileKey'].name, 'file.dat');
          assert_equals(files['FileKey'].size, 12);
        });
  }, 'Fetch with FormData body test');

promise_test(function(t) {
    return fetch('../resources/fetch-test-helpers.js')
      .then(function(res) { return res.text(); })
      .then(function(text) { assert_not_equals(text, ''); });
  }, 'Fetch a URL already in Blink cache');

test(function(t) {
    function runInfiniteFetchLoop() {
      fetch('dummy.html')
        .then(function() { runInfiniteFetchLoop(); });
    }
    runInfiniteFetchLoop();
  },
  'Destroying the execution context while fetch is happening should not ' +
  'cause a crash.');

test(t => {
    var req = new Request('/', {method: 'POST', body: ''});
    fetch(req);
    assert_true(req.bodyUsed);
}, 'Calling fetch() disturbs body if not null');

test(t => {
    var req = new Request('/', {method: 'POST'});
    fetch(req);
    assert_false(req.bodyUsed);
}, 'Calling fetch() doesn\'t disturb body if null');

done();
