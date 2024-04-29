# androidx.window.extensions

This library is not available as a prebuilt like other optional system
libraries in order to encourage clients to access it via the `androidx.window`
wrapper library. However, Chrome uses it directly in order to reduce the binary
size overhead of the wrapper library. Googlers, see: [b/233785784].

As with other optional SDK libraries, these classes are not compiled in. They
are available at runtime through a system library loaded by the framework
when a `<uses-library>` tag exists in an app's `AndroidManifest.xml` (which is
added via manifest merging when depending on the GN target).

Not all devices have this optional library available. You can check if it's
available via `org.chromium.window.WindowApiCheck#isAvailable()`.

Library documentation: https://source.android.com/docs/core/display/windowmanager-extensions

Library source code: https://cs.android.com/androidx/platform/frameworks/support/+/androidx-main:window/extensions/

[b/233785784]: http://b/233785784

## Updating the Sources

Run `update_sources.sh`
