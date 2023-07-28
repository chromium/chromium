description("Test resolution of relative URLs.");

cases = [ 
  // Format: [baseURL, relativeURL, expectedURL],
  // Basic absolute input.
  ["http://host/a", "http://another/", "http://another/"],
  ["http://host/a", "http:////another/", "http://another/"],
  // Empty relative URLs should only remove the ref part of the URL,
  // leaving the rest unchanged.
  ["http://foo/bar", "", "http://foo/bar"],
  ["http://foo/bar#ref", "", "http://foo/bar"],
  ["http://foo/bar#", "", "http://foo/bar"],
  // Spaces at the ends of the relative path should be ignored.
  ["http://foo/bar", "  another  ", "http://foo/another"],
  ["http://foo/bar", "  .  ", "http://foo/"],
  ["http://foo/bar", " \t ", "http://foo/bar"],
  // Matching schemes without two slashes are treated as relative.
  ["http://host/a", "http:path", "http://host/path"],
  ["http://host/a/", "http:path", "http://host/a/path"],
  ["http://host/a", "http:/path", "http://host/path"],
  ["http://host/a", "HTTP:/path", "http://host/path"],
  // Nonmatching schemes are absolute.
  ["http://host/a", "https:host2", "https://host2/"],
  ["http://host/a", "htto:/host2", "htto:/host2"],
  // Absolute path input
  ["http://host/a", "/b/c/d", "http://host/b/c/d"],
  ["http://host/a", "\\\\b\\\\c\\\\d", "http://host/b/c/d"],
  ["http://host/a", "/b/../c", "http://host/c"],
  ["http://host/a?b#c", "/b/../c", "http://host/c"],
  ["http://host/a", "\\\\b/../c?x#y", "http://host/c?x#y"],
  ["http://host/a?b#c", "/b/../c?x#y", "http://host/c?x#y"],
  // Relative path input
  ["http://host/a", "b", "http://host/b"],
  ["http://host/a", "bc/de", "http://host/bc/de"],
  ["http://host/a/", "bc/de?query#ref", "http://host/a/bc/de?query#ref"],
  ["http://host/a/", ".", "http://host/a/"],
  ["http://host/a/", "..", "http://host/"],
  ["http://host/a/", "./..", "http://host/"],
  ["http://host/a/", "../.", "http://host/"],
  ["http://host/a/", "././.", "http://host/a/"],
  ["http://host/a?query#ref", "../../../foo", "http://host/foo"],
  // Query input
  ["http://host/a", "?foo=bar", "http://host/a?foo=bar"],
  ["http://host/a?x=y#z", "?", "http://host/a?"],
  ["http://host/a?x=y#z", "?foo=bar#com", "http://host/a?foo=bar#com"],
  // Ref input
  ["http://host/a", "#ref", "http://host/a#ref"],
  ["http://host/a#b", "#", "http://host/a#"],
  ["http://host/a?foo=bar#hello", "#bye", "http://host/a?foo=bar#bye"],
  // Invalid schemes should be treated as relative.
  ["http://foo/bar", "./asd:fgh", "http://foo/asd:fgh"],
  ["http://foo/bar", ":foo", "http://foo/:foo"],
  ["http://foo/bar", " hello world", "http://foo/hello%20world"],
  // We should treat semicolons like any other character in URL resolving
  ["http://host/a", ";foo", "http://host/;foo"],
  ["http://host/a;", ";foo", "http://host/;foo"],
  ["http://host/a", ";/../bar", "http://host/bar"],
  // Relative URLs can also be written as "//foo/bar" which is relative to
  // the scheme. In this case, it would take the old scheme, so for http
  // the example would resolve to "http://foo/bar".
  ["http://host/a", "//another", "http://another/"],
  ["http://host/a", "//another/path?query#ref", "http://another/path?query#ref"],
  ["http://host/a", "///another/path", "http://another/path"],
  ["http://host/a", "//Another\\\\path", "http://another/path"],
  // Invalid URL since host is missing.
  ["http://host/a", "//", "//"],
  // IE will also allow one or the other to be a backslash to get the same
  // behavior.
  ["http://host/a", "\\\\/another/path", "http://another/path"],
  ["http://host/a", "/\\\\Another\\\\path", "http://another/path"],
];

var originalBaseURL = canonicalize(".");

for (var i = 0; i < cases.length; ++i) {
  baseURL = cases[i][0];
  relativeURL = cases[i][1];
  expectedURL = cases[i][2];
  setBaseURL(baseURL);
  shouldBe("canonicalize('" + relativeURL + "')",
           "'" + expectedURL + "'");
}

setBaseURL(originalBaseURL);
