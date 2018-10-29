// Test that Response methods which return a Promise don't crash after garbage
// collection. Crashes on failure. This function is extremely slow on MSAN
// builds. Each call should be a separate HTML file and added to SlowTests.
// See https://bugs.chromium.org/p/chromium/issues/detail?id=829790#c5

function testResponseMethod(methodName) {
  promise_test(async () => {
    const rs = new ReadableStream();
    const response = new Response(rs);
    try {
      // This throws an exception in debug builds but not in release builds.
      // If the process doesn't crash, the test passed.
      fillStackAndRun(() =>
                      Response.prototype[methodName].call(response).catch(() => {}),
                      784);
    } catch (e) {
    }
    await asyncGC();
  }, `stack overflow in response.${methodName}() should not crash the browser`);
}
