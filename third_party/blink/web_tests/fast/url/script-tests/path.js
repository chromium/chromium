description("Canonicalization of paths.");

cases = [ 
  ["/././foo", "/foo"],
  ["/./.foo", "/.foo"],
  ["/foo/.", "/foo/"],
  ["/foo/./", "/foo/"],
  // double dots followed by a slash or the end of the string count
  ["/foo/bar/..", "/foo/"],
  ["/foo/bar/../", "/foo/"],
  // don't count double dots when they aren't followed by a slash
  ["/foo/..bar", "/foo/..bar"],
  // some in the middle
  ["/foo/bar/../ton", "/foo/ton"],
  ["/foo/bar/../ton/../../a", "/a"],
  // we should not be able to go above the root
  ["/foo/../../..", "/"],
  ["/foo/../../../ton", "/ton"],
  // escaped dots should be unescaped and treated the same as dots
  ["/foo/%2e", "/foo/"],
  ["/foo/%2e%2", "/foo/.%2"],
  ["/foo/%2e./%2e%2e/.%2e/%2e.bar", "/..bar"],
  // Multiple slashes in a row should be preserved and treated like empty
  // directory names.
  ["////../..", "//"],
  ["/foo/bar//../..", "/foo/"],
  ["/foo/bar//..", "/foo/bar/"],
  ["/foo/bar/..", "/foo/"],

  // ----- escaping tests -----
  ["/foo", "/foo"],
  // Valid escape sequence
  ["/%20foo", "/%20foo"],
  // Invalid escape sequence we should pass through unchanged.
  ["/foo%", "/foo%"],
  ["/foo%2", "/foo%2"],
  // Invalid escape sequence: bad characters should be treated the same as
  // the sourrounding text, not as escaped (in this case, UTF-8).
  ["/foo%2zbar", "/foo%2zbar"],
  // (Disabled because requires UTF8)
  // ["/foo%2\xc2\xa9zbar", "/foo%2%C2%A9zbar"],
  ["/foo%2\u00c2\u00a9zbar", "/foo%2%C3%82%C2%A9zbar"],
  // Regular characters that are escaped should not be unescaped
  ["/foo%41%7a", "/foo%41%7a"],
  // Funny characters that are unescaped should be escaped
  ["/foo\u0009\u0091%91", "/foo%C2%91%91"],
  // Null character that is escaped should not cause a failure.
  ["/foo%00%51", "/foo%00%51"],
  // Some characters should be passed through unchanged regardless of esc.
  ["/(%28:%3A%29)", "/(%28:%3A%29)"],
  // Characters that are properly escaped should not have the case changed
  // of hex letters.
  ["/%3A%3a%3C%3c", "/%3A%3a%3C%3c"],
  // Funny characters that are unescaped should be escaped
  ["/foo\tbar", "/foobar"],
  // Backslashes should get converted to forward slashes
  ["\\\\foo\\\\bar", "/foo/bar"],
  // Hashes found in paths (possibly only when the caller explicitly sets
  // the path on an already-parsed URL) should be escaped.
  // (Disabled because requires ability to set path directly.)
  // ["/foo#bar", "/foo%23bar"],
  // %7f should be allowed and %3D should not be unescaped (these were wrong
  // in a previous version).
  ["/%7Ffp3%3Eju%3Dduvgw%3Dd", "/%7Ffp3%3Eju%3Dduvgw%3Dd"],
  // @ should be passed through unchanged (escaped or unescaped).
  ["/@asdf%40", "/@asdf%40"],

  // ----- encoding tests -----
  // Basic conversions
  ["/\u4f60\u597d\u4f60\u597d", "/%E4%BD%A0%E5%A5%BD%E4%BD%A0%E5%A5%BD"],
  // Unicode Noncharacter.
  ["/\ufdd0zyx", "/%EF%B7%90zyx"],
  // U+2025 TWO DOT LEADER should not be normalized to .. in the path
  ["/\u2025/foo", "/%E2%80%A5/foo"],
  // A half-surrogate is an error by itself U+DEAD
  // FIXME: ["/\uDEAD/foo", "/\uFFFD/foo"],
  // BOM code point with special meaning U+FEFF ZERO WIDTH NO-BREAK SPACE
  ["/\uFEFF/foo", "/%EF%BB%BF/foo"],
  // The BIDI override code points RLO and LRO
  ["/\u202E/foo/\u202D/bar", "/%E2%80%AE/foo/%E2%80%AD/bar"],
  // U+FF0F is an invalid host codepoint.
  ["\uFF0Ffoo/", ""],
];

for (var i = 0; i < cases.length; ++i) {
  test_vector = cases[i][0];
  expected_result = cases[i][1];
  if (!expected_result) {
    // The result of `canonicalize` should be same as the input if input is an
    // invalid URL.
    expected_result = test_vector;
  }
  shouldBe("canonicalize('http://example.com" + test_vector + "')",
           "'http://example.com" + expected_result + "'");
}
