//META: script=/resources/testdriver.js
//META: script=/resources/testdriver-vendor.js
//META: script=resources/font-asserts.js
//META: script=resources/font-data.js
//META: script=resources/font-test-utils.js

font_access_test(async t => {
  const testData = getTestData();
  assert_greater_than_equal(
      testData.size, 1, 'Need a least one test font data.');
  const testFont = testData.values().next().value;

  const queryInput = {
    select: [testFont.postscriptName]
  };
  const fonts = await navigator.fonts.query(queryInput);

  assert_equals(
      fonts.length, 1, 'The result length should match the test length.');
  assert_font_equals(fonts[0], testFont);
}, 'query(): existing postscript name in select');

font_access_test(async t => {
  const queryInput = {
    select: ['invalid_postscript_name']
  };
  const fonts = await navigator.fonts.query(queryInput);

  assert_equals(
      fonts.length, 0,
      'Fonts should not be selected for an invalid postscript name in select.');
}, 'query(): invalid postscript name in select');

font_access_test(async t => {
  const fonts = await navigator.fonts.query({});

  assert_greater_than_equal(
      fonts.length, 1,
      'All available fonts should be returned when an empty object is passed ' +
      'for input.');
}, 'query(): empty object');

font_access_test(async t => {
  const queryInput = {
    select: []
  };
  const fonts = await navigator.fonts.query(queryInput);

  assert_greater_than_equal(
      fonts.length, 1,
      'All available fonts should be returned when an empty array of select ' +
      'is passed for input.');
}, 'query(): empty select array');

font_access_test(async t => {
  // All fonts are expected to be queried when an invalid query parameter is
  // passed.
  const fonts = await navigator.fonts.query(undefined);

  assert_greater_than_equal(
      fonts.length, 1,
      'All available fonts should be returned when undefined is passed for ' +
      'input.');
}, 'query(): undefined input');

const non_ascii_input = [
  {select: ['¥']},
  {select: ['ß']},
  {select: ['🎵']},
  // UTF-16LE, encodes to the same first four bytes as "Ahem" in ASCII.
  {select: ['\u6841\u6d65']},
  // U+6C34 CJK UNIFIED IDEOGRAPH (water)
  {select: ['\u6C34']},
  // U+1D11E MUSICAL SYMBOL G-CLEF (UTF-16 surrogate pair)
  {select: ['\uD834\uDD1E']},
  // U+FFFD REPLACEMENT CHARACTER
  {select: ['\uFFFD']},
  // UTF-16 surrogate lead
  {select: ['\uD800']},
  // UTF-16 surrogate trail
  {select: ['\uDC00']},
];

for (const test of non_ascii_input) {
  font_access_test(async t => {
    const fonts = await navigator.fonts.query(test);
    assert_equals(
        fonts.length, 0,
        'Fonts should not be selected for non-ASCII character input: ' +
        JSON.stringify(fonts));
  }, `query(): non-ASCII character input: ${JSON.stringify(test)}`);
}