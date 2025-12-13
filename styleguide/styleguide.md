# Chromium coding style

## Main style guides

*   [Chromium C++ style guide](c++/c++.md)
    *   [Modern C++ use](c++/c++-features.md) for allowed/banned features.
    *   See also: [C++ Dos and Don'ts](c++/c++-dos-and-donts.md) for Chromium
        best-practices.
    *   [Blink C++ style](c++/blink-c++.md)
*   [Chromium Objective-C style guide](objective-c/objective-c.md)
*   [Chromium Rust style guide](rust/rust.md)
*   [Chromium Swift style guide](swift/swift.md)
*   [Java style guide for Android](java/java.md)
*   [Chromium Python style guide](python/python.md)
    *   [Blink Python style](python/blink-python.md)
*   [GN style guide](https://gn.googlesource.com/gn/+/main/docs/style_guide.md)
    for build files.
    *   See also: [Writing GN templates](../build/docs/writing_gn_templates.md)
        for Chromium best-practices.
*   [Markdown style guide](markdown/markdown.md)

Chromium also uses these languages to a lesser degree:

*   [Kernel C style](https://www.kernel.org/doc/html/latest/process/coding-style.html)
    for ChromiumOS firmware.
*   [WebIDL](https://www.chromium.org/blink/webidl/#syntax)
*   [Mojo IDL](../docs/security/mojo.md) for cross-process IPC
*   [Jinja style guide](https://sites.google.com/a/chromium.org/dev/developers/jinja#TOC-Style)
    for [Jinja](https://sites.google.com/a/chromium.org/dev/developers/jinja)
    templates.
*   [SQLite SQL style](../sql/README.md#SQL-style) for storage of cookies, etc.

Regardless of the language used, please keep code
[inclusive for all contributors](inclusive_code.md).

## Web languages (JavaScript, HTML, CSS)

When working on Web-based UI features, consult the
[Web Development Style Guide](web/web.md) for the Chromium conventions used in
JS/CSS/HTML files.

Internal uses of web languages, notably "layout" tests, should preferably follow
these style guides, but it is not enforced.
