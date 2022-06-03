if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
}

test(function() {
    var expectedValueMap = {
      'content-language': 'ja',
      'content-type': 'text/html; charset=UTF-8',
      'x-fetch-test': 'response test field'
    };

    var headers = new Headers;
    headers.set('Content-Language', 'ja');
    headers.set('Content-Type', 'text/html; charset=UTF-8');
    headers.set('X-Fetch-Test', 'text/html; charset=UTF-8');

    assert_equals(size(headers), 3, 'headers size should match');

    // 'has()', 'get()'
    var key = 'Content-Type';
    assert_true(headers.has(key));
    assert_true(headers.has(key.toUpperCase()));
    assert_equals(headers.get(key), expectedValueMap[key.toLowerCase()]);
    assert_equals(headers.get(key.toUpperCase()),
                  expectedValueMap[key.toLowerCase()]);
    assert_equals(headers.get('dummy'), null);
    assert_false(headers.has('dummy'));

    // 'delete()'
    var deleteKey = 'Content-Type';
    headers.delete(deleteKey);
    assert_equals(size(headers), 2, 'headers size should have -1 size');
    Object.keys(expectedValueMap).forEach(function(key) {
        if (key == deleteKey.toLowerCase())
          assert_false(headers.has(key));
        else
          assert_true(headers.has(key));
      });

    // 'set()'
    var testCasesForSet = [
      // For a new key/value pair.
      { key: 'Cache-Control',
        value: 'max-age=3600',
        isNewEntry: true },

      // For an existing key.
      { key: 'X-Fetch-Test',
        value: 'response test field - updated',
        isUpdate: true },

      // For setting a numeric value, expecting to see DOMString on getting.
      { key: 'X-Numeric-Value',
        value: 12345,
        expectedValue: '12345',
        isNewEntry: true },

      // For case-insensitivity test.
      { key: 'content-language',
        value: 'fi',
        isUpdate: true }
    ];

    var expectedHeaderSize = size(headers);
    testCasesForSet.forEach(function(testCase) {
        var key = testCase.key;
        var value = testCase.value;
        var expectedValue = ('expectedValue' in testCase) ?
          testCase.expectedValue : testCase.value;
        expectedHeaderSize = testCase.isNewEntry ?
          (expectedHeaderSize + 1) : expectedHeaderSize;

        headers.set(key, value);
        assert_true(headers.has(key));
        assert_equals(headers.get(key), expectedValue);
        if (testCase.isUpdate)
          assert_not_equals(headers.get(key), expectedValueMap[key.toLowerCase()]);
        assert_equals(size(headers), expectedHeaderSize);

        // Update expectedValueMap too for forEach() test below.
        expectedValueMap[key.toLowerCase()] = expectedValue;
      });

    // '[Symbol.iterator]()'
    for (var header of headers) {
      var key = header[0], value = header[1];
      assert_not_equals(key, deleteKey.toLowerCase());
      assert_true(key in expectedValueMap);
      assert_equals(headers.get(key), expectedValueMap[key]);
      assert_equals(value, expectedValueMap[key]);
    }

    // 'keys()'
    for (var key of headers.keys()) {
      assert_not_equals(key, deleteKey.toLowerCase());
      assert_true(key in expectedValueMap);
      assert_equals(headers.get(key), expectedValueMap[key]);
    }

    // 'values()'
    var expectedKeyMap = {};
    for (var key in expectedValueMap)
      expectedKeyMap[expectedValueMap[key]] = key;
    for (var value of headers.values()) {
      assert_true(value in expectedKeyMap);
      var key = expectedKeyMap[value];
      assert_not_equals(key, deleteKey.toLowerCase());
    }

    // 'entries()'
    var entries = [];
    for (var header of headers.entries()) {
      var key = header[0], value = header[1];
      assert_not_equals(key, deleteKey.toLowerCase());
      assert_true(key in expectedValueMap);
      assert_equals(headers.get(key), expectedValueMap[key]);
      assert_equals(value, expectedValueMap[key]);
      entries.push(header);
    }

    // 'forEach()'
    var thisObject = {};
    headers.forEach(function (value, key, headersObject) {
      var header = entries.shift();
      assert_equals(key, header[0]);
      assert_equals(value, header[1]);
      assert_equals(thisObject, this);
      assert_equals(headersObject, headers);
    }, thisObject);

    // 'append()'
    var allValues = headers.get('X-Fetch-Test').split(', ');
    assert_equals(allValues.length, 1);
    assert_equals(size(headers), 4);
    headers.append('X-FETCH-TEST', 'response test field - append');
    headers.append('X-FETCH-TEST-2', 'response test field - append');
    assert_equals(size(headers), 5, 'headers size should increase by 1.');
    assert_equals(headers.get('X-FETCH-Test'),
                  'response test field - updated, response test field - append',
                  'the value of the first header added should be returned.');
    allValues = headers.get('X-FETch-TEST').split(', ');
    assert_equals(allValues.length, 2);
    assert_equals(allValues[0], 'response test field - updated');
    assert_equals(allValues[1], 'response test field - append');
    headers.set('X-FETch-Test', 'response test field - set');
    assert_equals(size(headers), 5, 'the second header should be deleted');
    allValues = headers.get('X-Fetch-Test').split(', ');
    assert_equals(allValues.length, 1, 'the second header should be deleted');
    assert_equals(allValues[0], 'response test field - set');
    headers.append('X-Fetch-TEST', 'response test field - append');
    assert_equals(size(headers), 5, 'headers size should not increase by 1.');
    headers.delete('X-FeTCH-Test');
    assert_equals(size(headers), 4, 'two headers should be deleted.');

    // new Headers with sequence<sequence<ByteString>>
    headers = new Headers([['a', 'b'], ['c', 'd'], ['c', 'e']]);
    assert_equals(size(headers), 2, 'headers size should match');
    assert_equals(headers.get('a'), 'b');
    assert_equals(headers.get('c'), 'd, e');
    assert_equals(headers.get('c').split(', ')[0], 'd');
    assert_equals(headers.get('c').split(', ')[1], 'e');

    // new Headers with Headers
    var headers2 = new Headers(headers);
    assert_equals(size(headers2), 2, 'headers size should match');
    assert_equals(headers2.get('a'), 'b');
    assert_equals(headers2.get('c'), 'd, e');
    assert_equals(headers2.get('c').split(', ')[0], 'd');
    assert_equals(headers2.get('c').split(', ')[1], 'e');
    headers.set('a', 'x');
    assert_equals(headers.get('a'), 'x');
    assert_equals(headers2.get('a'), 'b');

    var headers3 = new Headers();
    headers3.append('test', 'a');
    headers3.append('test', '');
    headers3.append('test', 'b');
    assert_equals(headers3.get('test'), 'a, , b');
    headers3.set('test', '');
    assert_equals(headers3.get('test'), '');

    var headers4 = new Headers();
    headers4.append('foo', '');
    headers4.append('foo', 'a');
    assert_equals(headers4.get('foo'), ', a');
    // new Headers with Dictionary
    headers = new Headers({'a': 'b', 'c': 'd'});
    assert_equals(size(headers), 2, 'headers size should match');
    assert_equals(headers.get('a'), 'b');
    assert_equals(headers.get('c'), 'd');

    // Throw errors
    INVALID_HEADER_NAMES.forEach(function(name) {
        assert_throws_js(TypeError,
                         function() {
                           var obj = {};
                           obj[name] = 'a';
                           var headers = new Headers(obj);
                         },
                         'new Headers with an invalid name (' + name +
                         ') should throw');
        assert_throws_js(TypeError,
                         function() { var headers = new Headers([[name, 'a']]); },
                         'new Headers with an invalid name (' + name +
                         ') should throw');
      });


    INVALID_HEADER_VALUES.forEach(function(value) {
        assert_throws_js(TypeError,
                         function() { var headers = new Headers({'a': value}); },
                         'new Headers with an invalid value should throw');
        assert_throws_js(TypeError,
                         function() { var headers = new Headers([['a', value]]); },
                         'new Headers with an invalid value should throw');
      });

    assert_throws_js(TypeError,
                     function() { var headers = new Headers([[]]); },
                     'new Headers with a sequence with less than two strings ' +
                     'should throw');
    assert_throws_js(TypeError,
                     function() { var headers = new Headers([['a']]); },
                     'new Headers with a sequence with less than two strings ' +
                     'should throw');
    assert_throws_js(TypeError,
                     function() { var headers = new Headers([['a', 'b'], []]); },
                     'new Headers with a sequence with less than two strings ' +
                     'should throw');
    assert_throws_js(TypeError,
                     function() { var headers = new Headers([['a', 'b'],
                                                             ['x', 'y', 'z']]); },
                     'new Headers with a sequence with more than two strings ' +
                     'should throw');
  }, 'Headers');

test(function(t) {
    const headers = new Headers;
    headers.set('b', '1');
    headers.set('c', '2');
    headers.set('a', '3');

    const keys = [];
    for (let [key, value] of headers)
        keys.push(key);
    assert_array_equals(keys, ['a', 'b', 'c'],
                        'The pairs to iterate over should be sorted.');
}, 'Iteration order');

test(function(t) {
    const headers = new Headers;
    headers.set('a', '1');
    headers.set('b', '2');
    headers.set('c', '3');
    headers.append('a', '2');
    headers.append('a', '3');

    const iterator = headers.entries();

    headers.delete('a');
    headers.set('d', '4');

    const keys = [];
    const values = [];
    for (let [key, value] of iterator) {
        keys.push(key);
        values.push(value);
    }
    assert_array_equals(keys, ['a', 'b', 'c'],
                        'The pairs to iterate over should be the return ' +
                        'value of an algorithm that implicitly makes a copy.');
    assert_array_equals(values, ['1, 2, 3', '2', '3'],
                        "The values should be combined and separated by ','.");
}, 'Iteration mutation');

done();
