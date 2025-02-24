# OS XR Extensions

This library is not (yet) distributed with the Android SDK, and so has been
manually built and committed here.

As with other optional SDK libraries, these classes are not compiled in. They
are available at runtime through a system library loaded by the framework
when a `<uses-library>` tag exists in an app's `AndroidManifest.xml` (which is
added via manifest merging when depending on the GN target).

Not all devices have this optional library available.

Library source code: https://cs.android.com/androidx/platform/frameworks/support/+/androidx-main:xr/xr-stubs/
