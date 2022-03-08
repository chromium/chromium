'use strict';

function assert_font_equals(actualFont, expectedFont) {
  assert_equals(
      actualFont.postscriptName, expectedFont.postscriptName,
      `${actualFont.postscriptName}: postscriptName should match`);
  assert_equals(
      actualFont.fullName, expectedFont.fullName,
      `${actualFont.postscriptName}: fullName should match`);
  assert_equals(
      actualFont.family, expectedFont.family,
      `${actualFont.postscriptName}: family should match`);
  assert_equals(
      actualFont.style, expectedFont.style,
      `${actualFont.postscriptName}: style should match`);
}

function assert_font_has_tables(fontName, actualTables, expectedTables) {
  for (const expectedTable of expectedTables) {
    assert_equals(
        expectedTable.length, 4, 'Table names are always 4 characters long.');
    assert_true(
        actualTables.has(expectedTable),
        `Font ${fontName} did not have required table ${expectedTable}.`);
    assert_greater_than(
        actualTables.get(expectedTable).size, 0,
        `Font ${fontName} has table ${expectedTable} of size 0.`);
  }
}