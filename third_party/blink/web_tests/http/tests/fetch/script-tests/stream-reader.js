if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/streams/resources/rs-utils.js');
}

promise_test(function(t) {
    return fetch('/fetch/resources/doctype.html').then(function(res) {
        var stream = res.body;
        var reader = stream.getReader();
        assert_throws_js(TypeError, function() { stream.getReader() });
        reader.releaseLock();
        var another = stream.getReader();
        assert_not_equals(another, reader);
        another.releaseLock();
      });
  }, 'ReadableStreamReader acquisition / releasing');

promise_test(function(t) {
    return fetch('/fetch/resources/doctype.html').then(function(res) {
        var reader = res.body.getReader();
        return readableStreamToArray(res.body, reader);
      }).then(function(chunks) {
        var size = 0;
        for (var chunk of chunks) {
          assert_equals(chunk.constructor, Uint8Array, 'chunk\'s type');
          size += chunk.byteLength;
        }
        var buffer = new Uint8Array(size);
        var offset = 0;
        for (var chunk of chunks) {
          buffer.set(new Uint8Array(chunk), offset);
          offset += chunk.byteLength;
        }
        return new TextDecoder().decode(buffer);
      }).then(function(string) {
          assert_equals(string, '<!DOCTYPE html>\n');
      });
  }, 'read contents with ReadableStreamReader');

promise_test(() => {
    let reader;
    return fetch('/fetch/resources/progressive.php').then(res => {
        reader = res.body.getReader();
        return Promise.all([reader.read(), reader.read(), reader.read()]);
      }).then(() => {
        reader.releaseLock();
        // We expect the test finishes without crashing.
      });
  }, 'parallel read');

promise_test(function(t) {
    return fetch('/fetch/resources/progressive.php').then(function(res) {
        assert_false(res.bodyUsed);
        var reader = res.body.getReader();
        assert_false(res.bodyUsed);
        return res.text().then(unreached_fulfillment(t), function() {
            // text() should fail because the body is locked.
            // TODO(yhirano): Use finally once it gets available.
            reader.releaseLock();
          });
      });
  }, 'acquiring a reader should not set bodyUsed.');

promise_test(function(t) {
    var reader;
    var response;
    return fetch('/fetch/resources/progressive.php').then(function(res) {
        response = res;
        reader = res.body.getReader();
        return reader.read();
      }).then(() => {
        reader.releaseLock();
        assert_true(response.bodyUsed);
        assert_throws_js(TypeError, () => { response.clone(); });
      });
  }, 'Clone after reading');

promise_test(function(t) {
    return fetch('/fetch/resources/progressive.php').then(function(res) {
        res.body.cancel();
        assert_true(res.bodyUsed);
      });
  }, 'Cancelling stream stops downloading.');

promise_test(function(t) {
    var clone;
    return fetch('/fetch/resources/progressive.php').then(function(res) {
        clone = res.clone();
        res.body.cancel();
        assert_true(res.bodyUsed);
        assert_false(clone.bodyUsed);
        return clone.arrayBuffer();
      }).then(function(r) {
        assert_equals(r.byteLength, 190);
        assert_true(clone.bodyUsed);
      });
  }, 'Cancelling stream should not affect cloned one.');

promise_test(t => {
    let reader;
    return fetch('/fetch/resources/slow-failure.cgi').then(res => {
        reader = res.body.getReader();
        return readableStreamToArray(res.body, reader);
      }).then(unreached_fulfillment(t), e => {
        reader.releaseLock();
        assert_equals(e.name, 'TypeError');
      });
  }, 'Streaming error should be reported as a TypeError.');

done();
