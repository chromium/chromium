# Badging

This module contains the implementation of the [Badging API]. The implementation
is under [active development].

[Badging API]: https://github.com/WICG/badging
[active development]: https://crbug.com/719176

### API

See the [explainer] for details. The Badge interface is a member on Window
and exposes two static methods:

[explainer]: https://github.com/WICG/badging/blob/master/explainer.md

* `set(contents)`: Sets the associated app's badge as a "flag" (the argument
  is ignored).
* `clear()`: Sets the associated app's badge to nothing.

### Testing

`LayoutTests/badging/*.html` tests that the API accepts/rejects the appropriate
inputs (with a mock Mojo service). Testing at other layers will be added
during implementation.
