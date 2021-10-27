// Receives an image LargestContentfulPaint |entry| and checks |entry|'s attribute values.
// The |timeLowerBound| parameter is a lower bound on the loadTime value of the entry.
// The |options| parameter may contain some string values specifying the following:
// * 'renderTimeIs0': the renderTime should be 0 (image does not pass Timing-Allow-Origin checks).
//     When not present, the renderTime should not be 0 (image passes the checks).
// * 'sizeLowerBound': the |expectedSize| is only a lower bound on the size attribute value.
//     When not present, |expectedSize| must be exactly equal to the size attribute value.
function checkImage(entry, expectedUrl, expectedID, expectedSize, timeLowerBound, options = []) {
  assert_equals(entry.name, '', "Entry name should be the empty string");
  assert_equals(entry.entryType, 'largest-contentful-paint',
    "Entry type should be largest-contentful-paint");
  assert_equals(entry.duration, 0, "Entry duration should be 0");
  // The entry's url can be truncated.
  assert_equals(expectedUrl.substr(0, 100), entry.url.substr(0, 100),
    `Expected URL ${expectedUrl} should at least start with the entry's URL ${entry.url}`);
  assert_equals(entry.id, expectedID, "Entry ID matches expected one");
  assert_equals(entry.element, document.getElementById(expectedID),
    "Entry element is expected one");
  if (options.includes('renderTimeIs0')) {
    assert_equals(entry.renderTime, 0, 'renderTime should be 0');
    assert_between_exclusive(entry.loadTime, timeLowerBound, performance.now(),
      'loadTime should be between the lower bound and the current time');
    assert_equals(entry.startTime, entry.loadTime, 'startTime should equal loadTime');
  } else {
    assert_between_exclusive(entry.loadTime, timeLowerBound, entry.renderTime,
      'loadTime should occur between the lower bound and the renderTime');
    assert_greater_than_equal(performance.now(), entry.renderTime,
      'renderTime should occur before the entry is dispatched to the observer.');
    assert_equals(entry.startTime, entry.renderTime, 'startTime should equal renderTime');
  }
  if (options.includes('sizeLowerBound')) {
    assert_greater_than(entry.size, expectedSize);
  } else {
    assert_equals(entry.size, expectedSize);
  }
}
