# iOS Application Specific Services in Chrome for iOS

[UIActivity][1] is an abstract class for implementing app-specific services
such as Share (to social media), Print, Reading List, and Password Management
app extensions.

## Adding iOS Password Managers App Extensions

Chrome for iOS recognizes an action extension is a Password Manager app
extension in one of the following two ways.

1. By Bundle ID match. If the bundle ID for the app extension contains
the substring `find-login-action`, it will be handled as a Password
Manager app extension.

1. By explicitly listing in [`activity_type_util.mm`][2]. The anonymous
namespace function `IsPasswordManagerActivity()` in this file contains
a static structure listing all the Password Manager app extensions
known to Chrome for iOS. The first field is a string containing either
the full bundle ID or the leading portion of the bundle ID. The second
field is a flag to indicate whether a full bundle ID is expected or
if the string is intended to be a prefix for matching bundle IDs.

The first option is recommended because it does not require any code
changes to Chrome. If an app extension meets the first condition, it
works with current and previous versions of Chrome for iOS (since early
2016). If for any reasons that an app extension cannot change its
bundle ID, option 2 may be used. To add support to Chrome for iOS, submit
a changelist similar to [this][3] for review.


[1]: https://developer.apple.com/reference/uikit/uiactivity?language=objc
[2]: ./activity_type_util.mm
[3]: https://codereview.chromium.org/2820113002/
