description("Test resolution of relative Windows-like URLs.");

cases = [ 
  // Format: [baseURL, relativeURL, expectedURL],
  // Resolving against Windows file base URLs.
  ["file:///C:/foo", "http://host/", "http://host/"],
  ["file:///C:/foo", "bar", "file:///C:/bar"],
  ["file:///C:/foo", "../../../bar.html", "file:///C:/bar.html"],
  ["file:///C:/foo", "/../bar.html", "file:///C:/bar.html"],
  // But two backslashes on Windows should be UNC so should be treated
  // as absolute.
  ["http://host/a", "\\\\\\\\another\\\\path", ""],
  // IE doesn't support drive specs starting with two slashes. It fails
  // immediately and doesn't even try to load. We fix it up to either
  // an absolute path or UNC depending on what it looks like.
  ["file:///C:/something", "//c:/foo", "file:///C:/foo"],
  ["file:///C:/something", "//localhost/c:/foo", "file:///C:/foo"],
  // Windows drive specs should be allowed and treated as absolute.
  ["file:///C:/foo", "c:", ""],
  ["file:///C:/foo", "c:/foo", ""],
  ["http://host/a", "c:\\\\foo", ""],
  // Relative paths with drive letters should be allowed when the base is
  // also a file.
  ["file:///C:/foo", "/z:/bar", "file:///Z:/bar"],
  // Treat absolute paths as being off of the drive.
  ["file:///C:/foo", "/bar", "file:///C:/bar"],
  ["file://localhost/C:/foo", "/bar", "file:///C:/bar"],
  ["file:///C:/foo/com/", "/bar", "file:///C:/bar"],
  // On Windows, two slashes without a drive letter when the base is a file
  // means that the path is UNC.
  ["file:///C:/something", "//somehost/path", "file://somehost/path"],
  ["file:///C:/something", "/\\\\//somehost/path", "file://somehost/path"],
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
