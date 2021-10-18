// Test that a partitioned cookie set by |origin| with name |cookieName| is
// or is not sent in a request to |origin|.
//
// If |expectsCookie| is true, then the test cookie should be present in the
// request.
function testHttpPartitionedCookie({origin, cookieName, expectsCookie}) {
  promise_test(async () => {
    const resp = await credFetch(`${origin}/cookies/resources/list.py`);
    const cookies = await resp.json();
    assert_equals(
        cookies.hasOwnProperty(cookieName), expectsCookie,
        getPartitionedCookieAssertDesc(expectsCookie));
  }, getHttpPartitionedCookieTestName(expectsCookie));
}

function getHttpPartitionedCookieTestName(expectsCookie) {
  if (expectsCookie) {
    return 'HTTP partitioned cookie on the top-level site it was created in';
  }
  return 'HTTP partitioned cookie on a different top-level site';
}

function getPartitionedCookieAssertDesc(expectsCookie) {
  if (expectsCookie) {
    return 'Expected partitioned cookie to be available on the top-level ' +
        'site it was created in';
  }
  return 'Expected the partitioned cookie to not be available on a ' +
      'different top-level site';
}
