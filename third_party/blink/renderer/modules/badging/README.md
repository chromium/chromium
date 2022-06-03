# Badging

This module contains the implementation of the [Badging API].

[Badging API]: https://github.com/WICG/badging
[active development]: https://crbug.com/719176

### API

See the [explainer] for details. The NavigatorBadge mixin interface is
included by Navigator and WorkerNavigator, which exposes two methods:
setAppBadge() and clearAppBadge().

[explainer]: https://github.com/WICG/badging/blob/master/explainer.md

*  `setAppBadge(optional [EnforceRange] unsigned long long contents)`: Sets the
associated app's badge to |contents|.
   * When |contents| is omitted, sets the associated app's badge to "flag".
   * When |contents| is 0, sets the associated app's badge to nothing.
*  `clearAppBadge()`: Sets the associated app's badge to nothing.

### Testing

`web_tests/badging/*.html` tests that the API accepts/rejects the appropriate
inputs (with a mock Mojo service that verifies that the interface sends the
correct Mojo messages". These tests duplicate some of the tests from WPT (below)
but with internal verification.

`web_tests/external/wpt/badging/*.html` tests that the API accepts/rejects the
appropriate inputs. These tests do not test the internal state of the badge
(because they have no visibility into it), only success/failure.
