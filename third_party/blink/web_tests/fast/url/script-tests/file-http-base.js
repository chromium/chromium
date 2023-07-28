description("Canonicalization of file URLs when the base URL is an http URL");

cases = [ 
    // Windows-style paths
    ["file:c:\\\\foo\\\\bar.html", "file:///C:/foo/bar.html"],
    ["  File:c|////foo\\\\bar.html", "file:///C:////foo/bar.html"],
    ["file:", "file:///"],
    ["file:UNChost/path", "file://unchost/path"],
    // CanonicalizeFileURL supports absolute Windows style paths for IE
    // compatibility. Note that the caller must decide that this is a file
    // URL itself so it can call the file canonicalizer. This is usually
    // done automatically as part of relative URL resolving.
    ["c:\\\\foo\\\\bar", "file:///C:/foo/bar"],
    ["C|/foo/bar", "file:///C:/foo/bar"],
    ["/C|\\\\foo\\\\bar", "file:///C:/foo/bar"],
    ["//C|/foo/bar", "file:///C:/foo/bar"],
    ["//server/file", "file://server/file"],
    ["\\\\\\\\server\\\\file", "file://server/file"],
    ["/\\\\server/file", "file://server/file"],
    // We should preserve the number of slashes after the colon for IE
    // compatibility, except when there is none, in which case we should
    // add one.
    ["file:c:foo/bar.html", "file:///C:/foo/bar.html"],
    ["file:/\\\\/\\\\C:\\\\\\\\//foo\\\\bar.html", "file:///C:////foo/bar.html"],
    // Three slashes should be non-UNC, even if there is no drive spec (IE
    // does this, which makes the resulting request invalid).
    ["file:///foo/bar.txt", "file:///foo/bar.txt"],
    // TODO(brettw) we should probably fail for invalid host names, which
    // would change the expected result on this test. We also currently allow
    // colon even though it's probably invalid, because its currently the
    // "natural" result of the way the canonicalizer is written. There doesn't
    // seem to be a strong argument for why allowing it here would be bad, so
    // we just tolerate it and the load will fail later.
    ["FILE:/\\\\/\\\\7:\\\\\\\\//foo\\\\bar.html", "file://7:////foo/bar.html"],
    ["file:filer/home\\\\me", "file://filer/home/me"],
    // Make sure relative paths can't go above the "C:"
    ["file:///C:/foo/../../../bar.html", "file:///C:/bar.html"],
    // Busted refs shouldn't make the whole thing fail.
    ["file:///C:/asdf#\\xc2", "file:///C:/asdf#%C3%82"],
    ["file:///C:/asdf#\xc2", "file:///C:/asdf#%C3%82"],

    // Unix-style paths
    ["file:///home/me", "file:///home/me"],
    // Windowsy ones should get still treated as Unix-style.
    ["file:c:\\\\foo\\\\bar.html", "file:///c:/foo/bar.html"],
    ["file:c|//foo\\\\bar.html", "file:///c%7C//foo/bar.html"],
    // file: tests from WebKit (LayoutTests/fast/loader/url-parse-1.html)
    ["//", "//"],
    ["///", "///"],
    ["///test", "file:///test"],
    ["file://test", "file://test/"],
    ["file://localhost",  "file://localhost/"],
    ["file://localhost/", "file://localhost/"],
    ["file://localhost/test", "file://localhost/test"],
];

var originalBaseURL = canonicalize(".");
setBaseURL("http://example.com/mock/path");

for (var i = 0; i < cases.length; ++i) {
  test_vector = cases[i][0];
  expected_result = cases[i][1];
  shouldBe("canonicalize('" + test_vector + "')",
           "'" + expected_result + "'");
}

setBaseURL(originalBaseURL);
