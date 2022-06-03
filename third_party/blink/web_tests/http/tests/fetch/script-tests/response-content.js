if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
}

promise_test(function() {
    var response = new Response;
    return response.text()
      .then(function(text) {
          assert_equals(text, '',
                        'response.text() must return an empty string' +
                        'if body is null');
        });
  }, 'Behavior of Response with no constructor arguments.');

promise_test(function() {
    var response = new Response('test string');
    assert_equals(
      response.headers.get('Content-Type'),
      'text/plain;charset=UTF-8',
      'A Response constructed with a string should have a Content-Type.');
    return response.text()
      .then(function(text) {
          assert_equals(text, 'test string',
                        'Response body text should match the string on ' +
                        'construction.');
        });
  }, 'Behavior of Response with string content.');

promise_test(function() {
    var intView = new Int32Array([0, 1, 2, 3, 4, 55, 6, 7, 8, 9]);
    var buffer = intView.buffer;

    var response = new Response(buffer);
    assert_false(response.headers.has('Content-Type'),
                 'A Response constructed with ArrayBuffer should not have a ' +
                 'content type.');
    return response.arrayBuffer()
      .then(function(buffer) {
          var resultIntView = new Int32Array(buffer);
          assert_array_equals(
            resultIntView, [0, 1, 2, 3, 4, 55, 6, 7, 8, 9],
            'Response body ArrayBuffer should match ArrayBuffer ' +
            'it was constructed with.');
        });
  }, 'Behavior of Response with ArrayBuffer content.');

promise_test(function() {
    var intView = new Int32Array([0, 1, 2, 3, 4, 55, 6, 7, 8, 9]);

    var response = new Response(intView);
    assert_false(response.headers.has('Content-Type'),
                 'A Response constructed with ArrayBufferView ' +
                 'should not have a content type.');
    return response.arrayBuffer()
      .then(function(buffer) {
          var resultIntView = new Int32Array(buffer);
          assert_array_equals(
            resultIntView, [0, 1, 2, 3, 4, 55, 6, 7, 8, 9],
            'Response body ArrayBuffer should match ArrayBufferView ' +
            'it was constructed with.');
        });
  }, 'Behavior of Response with ArrayBufferView content without a slice.');

promise_test(function() {
    var intView = new Int32Array([0, 1, 2, 3, 4, 55, 6, 7, 8, 9]);
    var slice = intView.subarray(1, 4);  // Should be [1, 2, 3]
    var response = new Response(slice);
    assert_false(response.headers.has('Content-Type'),
                 'A Response constructed with ArrayBufferView ' +
                 'should not have a content type.');
    return response.arrayBuffer()
      .then(function(buffer) {
          var resultIntView = new Int32Array(buffer);
          assert_array_equals(
            resultIntView, [1, 2, 3],
            'Response body ArrayBuffer should match ArrayBufferView ' +
            'slice it was constructed with.');
        });
  }, 'Behavior of Response with ArrayBufferView content with a slice.');

promise_test(function(t) {
    var formData = new FormData();
    formData.append('sample string', '1234567890');
    formData.append('sample blob', new Blob(['blob content']));
    formData.append('sample file',
                    new File(['file content'], 'file.dat'));
    var response = new Response(formData);
    return response.text()
      .then(function(result) {
          var reg = new RegExp('multipart\/form-data; boundary=(.*)');
          var regResult = reg.exec(getContentType(response.headers));
          var boundary = regResult[1];
          var expected_body =
            '--' + boundary + '\r\n' +
            'Content-Disposition: form-data; name="sample string"\r\n' +
            '\r\n' +
            '1234567890\r\n' +
            '--' + boundary + '\r\n' +
            'Content-Disposition: form-data; name="sample blob"; ' +
            'filename="blob"\r\n' +
            'Content-Type: application/octet-stream\r\n' +
            '\r\n' +
            'blob content\r\n' +
            '--' + boundary + '\r\n' +
            'Content-Disposition: form-data; name="sample file"; ' +
            'filename="file.dat"\r\n' +
            'Content-Type: application/octet-stream\r\n' +
            '\r\n' +
            'file content\r\n' +
            '--' + boundary + '--\r\n';
          assert_equals(
            result, expected_body,
            'Creating a Response with FormData body must succeed.');
          response = new Response(
            expected_body, {headers: [['Content-Type', regResult[0]]]});
          return response.formData();
        })
      .then(function(result) {
          assert_equals(result.get('sample string'),
                        formData.get('sample string'));
          assert_equals(result.get('sample blob').name,
                        formData.get('sample blob').name);
          assert_equals(result.get('sample blob').size,
                        formData.get('sample blob').size);
          assert_equals(result.get('sample blob').type,
                        'application/octet-stream');
          assert_equals(result.get('sample file').name,
                        formData.get('sample file').name);
          assert_equals(result.get('sample file').size,
                        formData.get('sample file').size);
          assert_equals(result.get('sample file').type,
                        'application/octet-stream');
        });
  }, 'Behavior of Response with FormData content');

promise_test(function() {
    const urlSearchParams = new URLSearchParams();
    urlSearchParams.append('sample string', '1234567890');
    urlSearchParams.append('sample string 2', '1234567890 & 2');
    var response = new Response(urlSearchParams);
    assert_equals(
      response.headers.get('Content-Type'),
      'application/x-www-form-urlencoded;charset=UTF-8',
      'A Response constructed with a URLSearchParams should have a Content-Type.');
    return response.text()
      .then(function(result) {
          const expected_body =
            'sample+string=1234567890&sample+string+2=1234567890+%26+2';
          assert_equals(
            result, expected_body,
            'Creating a Response with URLSearchParams body must succeed.');
          response = new Response(
            expected_body, {headers: [
              ['Content-Type',
               'application/x-www-form-urlencoded; charset=UTF-8']]});
          return response.formData();
        })
      .then(function(result) {
          assert_equals(result.get('sample string'),
                        urlSearchParams.get('sample string'));
          assert_equals(result.get('sample string 2'),
                        urlSearchParams.get('sample string 2'));
        });
  }, 'Behavior of Response with URLSearchParams content');

promise_test(function() {
    var headers = new Headers;
    headers.set('Content-Language', 'ja');
    var response = new Response(
      'test string', {method: 'GET', headers: headers});
    assert_false(response.bodyUsed);
    var response2 = response.clone();
    assert_false(response.bodyUsed, 'bodyUsed is not set by clone().');
    assert_false(response2.bodyUsed, 'bodyUsed is not set by clone().');
    response.headers.set('Content-Language', 'en');
    assert_equals(
      response2.headers.get('Content-Language'), 'ja', 'Headers of cloned ' +
      'response should not change when original response headers are changed.');

    var p = response.text();
    assert_true(response.bodyUsed, 'bodyUsed should be true when locked.');
    assert_false(response2.bodyUsed,
                 'Cloned bodies should not share bodyUsed.');
    assert_throws_js(TypeError,
                     function() { response3 = response.clone(); },
                     'Response.clone() should throw if the body was used.');
    return p.then(function(text) {
        assert_true(response.bodyUsed);
        assert_false(response2.bodyUsed);
        return response2.text();
      }).then(function(text) {
        assert_equals(text, 'test string',
                      'Response clone response body text should match.');
        assert_true(response2.bodyUsed);
      });
  }, 'Behavior of bodyUsed in Response and clone behavior.');

promise_test(function() {
    var response = new Response(null);
    assert_equals(
      response.headers.get('Content-Type'),
      null,
      'A Response constructed with null body should have no Content-Type.');
    return response.text()
      .then(function(text) {
          assert_equals(text, '',
                        'Response with null body accessed as text should ' +
                        'resolve to the empty string.');
        });
  }, 'Behavior of Response passed null for body.');

promise_test(function() {
    var response = new Response();
    assert_equals(
      response.headers.get('Content-Type'),
      null,
      'A Response constructed with no body should have no Content-Type.');
    return response.text()
      .then(function(text) {
          assert_equals(text, '',
                        'Response with no body accessed as text should ' +
                        'resolve to the empty string.');
        });
  }, 'Behavior of Response with no body.');

done();
