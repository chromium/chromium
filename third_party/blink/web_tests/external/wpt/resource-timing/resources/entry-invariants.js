// Asserts that the given attributes are present in 'entry' and hold values
// that are sorted in the same order as given in 'attributes'.
const assert_ordered_ = (entry, attributes) => {
  let before = attributes[0];
  attributes.slice(1).forEach(after => {
    assert_greater_than_equal(entry[after], entry[before],
      `${after} should be greater than ${before}`);
    before = after;
  });
}

// Asserts that the given attributes are present in 'entry' and hold a value of
// 0.
const assert_zeroed_ = (entry, attributes) => {
  attributes.forEach(attribute => {
    assert_equals(entry[attribute], 0, `${attribute} should be 0`);
  });
}

// Asserts that the given attributes are present in 'entry' and hold a value of
// 0 or more.
const assert_not_negative_ = (entry, attributes) => {
  attributes.forEach(attribute => {
    assert_greater_than_equal(entry[attribute], 0,
      `${attribute} should be greater than or equal to 0`);
  });
}

// Asserts that the given attributes are present in 'entry' and hold a value
// greater than 0.
const assert_positive_ = (entry, attributes) => {
  attributes.forEach(attribute => {
    assert_greater_than(entry[attribute], 0,
      `${attribute} should be greater than 0`);
  });
}

const invariants = {
  // Asserts that attributes of the given PerformanceResourceTiming entry match
  // what the spec dictates for any resource fetched over HTTP.
  assert_http_resource: entry => {
    assert_ordered_(entry, [
      "fetchStart",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
      "connectEnd",
      "requestStart",
      "responseStart",
      "responseEnd",
    ]);

    assert_zeroed_(entry, [
      "workerStart",
      "secureConnectionStart",
      "redirectStart",
      "redirectEnd",
    ]);

    assert_not_negative_(entry, [
      "duration",
    ]);

    assert_positive_(entry, [
      "fetchStart",
      "transferSize",
      "encodedBodySize",
      "decodedBodySize",
    ]);
  },

  // Asserts that attributes of the given PerformanceResourceTiming entry match
  // what the spec dictates for any resource fetched over HTTP through an HTTP
  // redirect.
  assert_same_origin_redirected_resource: entry => {
    assert_positive_(entry, [
      "redirectStart",
    ]);

    assert_equals(entry.redirectStart, entry.startTime,
      "redirectStart should be equal to startTime");

    assert_ordered_(entry, [
      "redirectStart",
      "redirectEnd",
      "fetchStart",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
    ]);
  },

  // Asserts that attributes of the given PerformanceResourceTiming entry match
  // what the spec dictates for any resource fetched over HTTPS through a
  // cross-origin redirect.
  // (e.g. GET http://remote.com/foo => 304 Location: https://remote.com/foo)
  assert_cross_origin_redirected_resource: entry => {
    assert_zeroed_(entry, [
      "redirectStart",
      "redirectEnd",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
      "connectEnd",
      "secureConnectionStart",
      "requestStart",
      "responseStart",
    ]);

    assert_positive_(entry, [
      "fetchStart",
      "responseEnd",
    ]);

    assert_ordered_(entry, [
      "fetchStart",
      "responseEnd",
    ]);
  },

  // Asserts that attributes of the given PerformanceResourceTiming entry match
  // what the spec dictates for a resource fetched over HTTPS through a
  // TAO enabled cross-origin redirect.
  assert_tao_enabled_cross_origin_redirected_resource: entry => {
    assert_positive_(entry, [
      "redirectStart",
    ]);
    assert_ordered_(entry, [
      "redirectStart",
      "redirectEnd",
      "fetchStart",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
      "secureConnectionStart",
      "connectEnd",
      "requestStart",
      "responseStart",
      "responseEnd",
    ]);
  },
};

// Given a resource-loader and a PerformanceResourceTiming validator, loads a
// resource and validates the resulting entry.
const attribute_test = (load_resource, validate, test_label) => {
  promise_test(
    async () => {
      // Clear out everything that isn't the one ResourceTiming entry under test.
      performance.clearResourceTimings();

      await load_resource();

      const entry_list = performance.getEntriesByType("resource");
      if (entry_list.length != 1) {
        const names = entry_list
          .map(e => e.name)
          .join(", ");
        throw new Error(`There should be one entry for one resource (found ${entry_list.length}: ${names})`);
      }

      validate(entry_list[0]);
  }, test_label);
}
