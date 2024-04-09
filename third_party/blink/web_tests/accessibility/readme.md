# Web Tests for Accessibility

## General Info on web tests: Building and Running the Tests

See [Web Tests](/docs/testing/web_tests.md) for general
info on how to build and run web tests.

## Old vs. New

There are two styles of accessibility web tests:

* Using a ```-expected.txt``` (now deprecated)
* Unit-style tests with assertions

Use the unit-style tests. An example is aria-modal.html.

## Methodology and Bindings

These tests check the accessibility tree directly in Blink using ```AccessibilityController```, which is just a test helper.

The code that implements the bindings is here:

* ```content/web_test/renderer/accessibility_controller.cc```
* ```content/web_test/renderer/web_ax_object_proxy.cc```

You'll probably find bindings for the features you want to test already. If not, it's not hard to add new ones.
