if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
}

// Tests that invalid names/values throw TypeError.
function testInvalidNamesAndValues(headers) {
  INVALID_HEADER_NAMES.forEach(function(name) {
      assert_throws_js(TypeError,
                       function() { headers.append(name, 'a'); },
                       'Headers.append with an invalid name (' + name +
                       ') must throw');
      assert_throws_js(TypeError,
                       function() { headers.delete(name); },
                       'Headers.delete with an invalid name (' + name +
                       ') must throw');
      assert_throws_js(TypeError,
                       function() { headers.get(name); },
                       'Headers.get with an invalid name (' + name +
                       ') must throw');
      assert_throws_js(TypeError,
                       function() { headers.getAll(name); },
                       'Headers.getAll with an invalid name (' + name +
                       ') must throw');
      assert_throws_js(TypeError,
                       function() { headers.has(name); },
                       'Headers.has with an invalid name (' + name +
                       ') must throw');
      assert_throws_js(TypeError,
                       function() { headers.set(name, 'a'); },
                       'Headers.set with an invalid name (' + name +
                       ') must throw');
    });
  INVALID_HEADER_VALUES.forEach(function(value) {
      assert_throws_js(TypeError,
                       function() { headers.append('a', value); },
                       'Headers.append with an invalid value must throw');
      assert_throws_js(TypeError,
                       function() { headers.set('a', value); },
                       'Headers.set with an invalid value must throw');
    });
}

// Tests that |header| is append()ed to/set() to/delete()d from |headers|.
function testAcceptHeaderName(headers, header) {
  headers.append(header, 'test');
  assert_equals(headers.get(header), 'test',
                header + ' must be append()ed.');

  headers.set(header, 'test2');
  assert_equals(headers.get(header), 'test2',
                header + ' must be overwritten by set().');

  headers.delete(header);
  assert_equals(headers.get(header), null,
                header + ' must be delete()d.');

  headers.set(header, 'test3');
  assert_equals(headers.get(header), 'test3',
                header + ' must be created by set().');

  headers.delete(header);
  assert_equals(headers.get(header), null,
                header + ' must be delete()d.');
}

// Tests that |header| is ignored in append()/set()/delete() of |headers|.
function testIgnoreHeaderName(headers, header) {
  headers.append(header, 'test');
  assert_equals(headers.get(header), null,
                header + ' must be ignored in append().');

  headers.set(header, 'test2');
  assert_equals(headers.get(header), null,
                header + ' must be ignored in set().');

  // Calling delete() does not crash nor throw an exception.
  headers.delete(header);
}

test(function() {
    [new Headers(),
     new Headers((new Request(URL, {mode: 'same-origin'})).headers),
     new Headers((new Response()).headers)]
      .forEach(function(headers) {
          testInvalidNamesAndValues(headers);

          FORBIDDEN_HEADER_NAMES
            .concat(FORBIDDEN_RESPONSE_HEADER_NAMES)
            .concat(SIMPLE_HEADER_NAMES)
            .concat([CONTENT_TYPE])
            .concat(NON_SIMPLE_HEADER_NAMES)
            .forEach(function(header) {
                testAcceptHeaderName(headers, header);
              });
        });
  }, 'Headers guard = none: set/append/delete');

test(function() {
    FORBIDDEN_HEADER_NAMES
      .concat(FORBIDDEN_RESPONSE_HEADER_NAMES)
      .concat(SIMPLE_HEADER_NAMES)
      .concat([CONTENT_TYPE])
      .concat(NON_SIMPLE_HEADER_NAMES)
      .forEach(function(header) {
          var headers = new Headers([[header, 'test']]);
          assert_equals(headers.get(header), 'test',
                        header + ' must be set (1)');
          var headers2 = new Headers(headers);
          assert_equals(headers2.get(header), 'test',
                        header + ' must be set (2)');
      });
  }, 'Headers guard = none: Headers constructor');

test(function() {
    [(new Request(URL, {mode: 'same-origin'})).headers,
     (new Request(URL, {mode: 'same-origin'})).clone().headers,
     (new Request(URL, {mode: 'cors'})).headers,
     (new Request(URL, {mode: 'cors'})).clone().headers]
      .forEach(function(headers) {
          testInvalidNamesAndValues(headers);

          // Forbidden header names must be ignored.
          FORBIDDEN_HEADER_NAMES
            .forEach(function(header) {
                testIgnoreHeaderName(headers, header);
              });

          // Other header names must be accepted.
          FORBIDDEN_RESPONSE_HEADER_NAMES
            .concat(SIMPLE_HEADER_NAMES)
            .concat([CONTENT_TYPE])
            .concat(NON_SIMPLE_HEADER_NAMES)
            .forEach(function(header) {
                testAcceptHeaderName(headers, header);
              });
        });
  }, 'Headers guard = request: set/append/delete');

test(function() {
    ['same-origin', 'cors'].forEach(function(mode) {
        // Forbidden header names must be ignored.
        FORBIDDEN_HEADER_NAMES
          .forEach(function(header) {
              var headers = (new Request('http://localhost/',
                                         {headers: [[header, 'test']],
                                          mode: mode})).headers;
              assert_equals(headers.get(header), null,
                            header + ' must be ignored (1)');

              headers = (new Request('http://localhost/',
                                     {headers: new Headers([[header, 'test']]),
                                      mode: mode})).headers;
              assert_equals(headers.get(header), null,
                            header + ' must be ignored (2)');
            });

        // Other header names must be accepted.
        FORBIDDEN_RESPONSE_HEADER_NAMES
          .concat(SIMPLE_HEADER_NAMES)
          .concat([CONTENT_TYPE])
          .concat(NON_SIMPLE_HEADER_NAMES)
          .forEach(function(header) {
              var headers = (new Request('http://localhost/',
                                         {headers: [[header, 'test']],
                                          mode: mode})).headers;
              assert_equals(headers.get(header), 'test',
                            header + ' must be set (1)');

              headers = (new Request('http://localhost/',
                                     {headers: new Headers([[header, 'test']]),
                                      mode: mode})).headers;
              assert_equals(headers.get(header), 'test',
                            header + ' must be set (2)');
            });
      });
  }, 'Headers guard = request: RequestInit.headers');

test(function() {
    [function() {return (new Request(URL, {mode: 'no-cors'})).headers;},
     function() {return (new Request(URL, {mode: 'no-cors'})).clone().headers;}]
      .forEach(function(createNewHeaders) {
        var headers = createNewHeaders();
        testInvalidNamesAndValues(headers);

        // Non-simple headers must be ignored.
        FORBIDDEN_HEADER_NAMES
          .concat(FORBIDDEN_RESPONSE_HEADER_NAMES)
          .concat(NON_SIMPLE_HEADER_NAMES)
          .forEach(function(header) {
              testIgnoreHeaderName(headers, header);
            });

        // SIMPLE_HEADER_NAMES must be accepted.
        SIMPLE_HEADER_NAMES
          .forEach(function(header) {
              testAcceptHeaderName(headers, header);
            });

        // Content-Type with NON_SIMPLE_HEADER_CONTENT_TYPE_VALUES are
        // ignored in append()/set().
        NON_SIMPLE_HEADER_CONTENT_TYPE_VALUES
          .forEach(function(value) {
              headers.append(CONTENT_TYPE, value);
              assert_equals(headers.get(CONTENT_TYPE), null,
                            'Content-Type/' + value +
                            ' must be ignored in append()');
              headers.set(CONTENT_TYPE, value);
              assert_equals(headers.get(CONTENT_TYPE), null,
                            'Content-Type/' + value +
                            ' must be ignored in set()');
            });

        // Content-Type with values in SIMPLE_HEADER_CONTENT_TYPE_VALUES are
        // accepted by append()/set(), but not delete().
        SIMPLE_HEADER_CONTENT_TYPE_VALUES
          .forEach(function(value) {
              var headers = createNewHeaders();
              headers.append(CONTENT_TYPE, value);
              assert_equals(headers.get(CONTENT_TYPE), value,
                            'Content-Type/' + value +
                            ' must be appended.');

              headers.delete(CONTENT_TYPE);
              assert_equals(headers.get(CONTENT_TYPE), null,
                            'Content-type/' + value +
                            ' must be deleted.');

              var headers = createNewHeaders();
              headers.set(CONTENT_TYPE, value);
              assert_equals(headers.get(CONTENT_TYPE), value,
                            'Content-Type/' + value +
                            ' must be set.');
            });
      });
  }, 'Headers guard = request-no-cors: set/append/delete');

test(function() {
    // Non-simple headers must be ignored.
    FORBIDDEN_HEADER_NAMES
      .concat(FORBIDDEN_RESPONSE_HEADER_NAMES)
      .concat(NON_SIMPLE_HEADER_NAMES)
      .forEach(function(header) {
          var headers = (new Request('http://localhost/',
                                     {headers: [[header, 'test']],
                                      mode: 'no-cors'})).headers;
          assert_equals(headers.get(header), null,
                        header + ' must be ignored (1)');

          headers = (new Request('http://localhost/',
                                 {headers: new Headers([[header, 'test']]),
                                  mode: 'no-cors'})).headers;
          assert_equals(headers.get(header), null,
                        header + ' must be ignored (2)');
        });

    // SIMPLE_HEADER_NAMES must be accepted.
    SIMPLE_HEADER_NAMES.forEach(function(header) {
        var headers = (new Request('http://localhost/',
                                   {headers: [[header, 'test']],
                                    mode: 'no-cors'
                                   })).headers;
        assert_equals(headers.get(header), 'test',
                      header + ' must be set (1)');

        headers = (new Request('http://localhost/',
                               {headers: new Headers([[header, 'test']]),
                                mode: 'no-cors'})).headers;
        assert_equals(headers.get(header), 'test',
                      header + ' must be set (2)');
      });

    // Content-Type with values in SIMPLE_HEADER_CONTENT_TYPE_VALUES must be
    // accepted.
    SIMPLE_HEADER_CONTENT_TYPE_VALUES.forEach(function(value) {
        var headers = (new Request('http://localhost/',
                                   {headers: [[CONTENT_TYPE, value]],
                                    mode: 'no-cors'
                                   })).headers;
        assert_equals(headers.get(CONTENT_TYPE), value,
                      CONTENT_TYPE + '/' + value + ' must be set (1)');

        headers = (new Request('http://localhost/',
                               {headers: new Headers([[CONTENT_TYPE, value]]),
                                mode: 'no-cors'})).headers;
        assert_equals(headers.get(CONTENT_TYPE), value,
                      CONTENT_TYPE + '/' + value + ' must be set (2)');
      });

    // Content-Type with NON_SIMPLE_HEADER_CONTENT_TYPE_VALUES must be
    // ignored.
    NON_SIMPLE_HEADER_CONTENT_TYPE_VALUES.forEach(function(value) {
        var headers = (new Request('http://localhost/',
                                   {headers: [[CONTENT_TYPE, value]],
                                    mode: 'no-cors'
                                   })).headers;
        assert_equals(headers.get(CONTENT_TYPE), null,
                      CONTENT_TYPE + '/' + value + ' must be ignored (1)');

        headers = (new Request('http://localhost/',
                               {headers: new Headers([[CONTENT_TYPE, value]]),
                                mode: 'no-cors'})).headers;
        assert_equals(headers.get(CONTENT_TYPE), null,
                      CONTENT_TYPE + '/' + value + ' must be ignored (2)');
      });
  }, 'Headers guard = request-no-cors: RequestInit.headers');

test(function() {
    [(new Response(new Blob(['']))).headers,
     (new Response(new Blob(['']))).clone().headers]
      .forEach(function(headers) {
          testInvalidNamesAndValues(headers);

          // Forbidden response header names must be ignored.
          FORBIDDEN_RESPONSE_HEADER_NAMES
            .forEach(function(header) {
                testIgnoreHeaderName(headers, header);
              });

          // Other header names must be accepted.
          FORBIDDEN_HEADER_NAMES
            .concat(SIMPLE_HEADER_NAMES)
            .concat([CONTENT_TYPE])
            .concat(NON_SIMPLE_HEADER_NAMES)
            .forEach(function(header) {
                testAcceptHeaderName(headers, header);
              });
        });
  }, 'Headers guard = response: set/append/delete');

test(function() {
    // Forbidden response header names must be ignored.
    FORBIDDEN_RESPONSE_HEADER_NAMES
      .forEach(function(header) {
          var headers = (new Response(new Blob(),
                                      {headers: [[header, 'test']]})).headers;
          assert_equals(headers.get(header), null,
                        header + ' must be ignored (1)');

          var headers = (new Response(new Blob(),
                                      {headers: new Headers([[header, 'test']])}
                                     )).headers;
          assert_equals(headers.get(header), null,
                        header + ' must be ignored (2)');
        });

    // Other header names must be accepted.
    FORBIDDEN_HEADER_NAMES
      .concat(SIMPLE_HEADER_NAMES)
      .concat([CONTENT_TYPE])
      .concat(NON_SIMPLE_HEADER_NAMES)
      .forEach(function(header) {
          var headers = (new Response(new Blob(),
                                      {headers: [[header, 'test']]})).headers;
          assert_equals(headers.get(header), 'test',
                        header + ' must be set (1)');

          headers = (new Response(new Blob(),
                                  {headers: new Headers([[header, 'test']])}
                                 )).headers;
          assert_equals(headers.get(header), 'test',
                        header + ' must be set (2)');
        });
  }, 'Headers guard = response: ResponseInit.headers');

promise_test(function(t) {
  return fetch('../resources/doctype.html')
    .then(function(res) {
        [res.headers,
         res.clone().headers,
         Response.error().headers,
         Response.error().clone().headers,
         Response.redirect('https://www.example.com/test.html').headers,
         Response.redirect('https://www.example.com/test.html').clone().headers
        ].forEach(function(headers) {
            testInvalidNamesAndValues(headers);

            // Test that TypeError is thrown for all header names.
            FORBIDDEN_HEADER_NAMES
              .concat(FORBIDDEN_RESPONSE_HEADER_NAMES)
              .concat(SIMPLE_HEADER_NAMES)
              .concat([CONTENT_TYPE])
              .concat(NON_SIMPLE_HEADER_NAMES)
              .forEach(function(header) {
                  var value = headers.get(header);

                  assert_throws_js(TypeError,
                                   function() { headers.append(header, 'test'); },
                                   'Headers.append(' + header + ') must throw');
                  assert_equals(headers.get(header), value,
                    'header ' + header + ' must be unchanged by append()');

                  assert_throws_js(TypeError,
                                   function() { headers.set(header, 'test'); },
                                   'Headers.set(' + header + ') must throw');
                  assert_equals(headers.get(header), value,
                    'header ' + header + ' must be unchanged by set()');

                  assert_throws_js(TypeError,
                                   function() { headers.delete(header); },
                                   'Headers.delete(' + header + ') must throw');
                  assert_equals(headers.get(header), value,
                    'header ' + header + ' must be unchanged by delete()');
                });
          });
      });
  }, 'Headers guard = immutable: set/append/delete');

done();
