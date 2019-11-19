# WebView Policies

## Overview

WebView supports a subset of Chrome's policies, that can be set via the
[App Restrictions][1] mechanism. They are read on the embedder app itself rather
than from a centralized location. That gives administrator more flexibility and
granularity, but to apply a global policy, it has to be set separately
on each app.

The policies will be applied to WebViews used inside the targeted apps without
having to modify the apps themselves. No special WebView APIs have been added
to expose policy information. If developers want to change their app's behavior
depending on that, they can [read them][2] as they have access to the
App Restrictions.

Please see the [Policy List on chromium.org][2] for more information and the
list of supported policies.

[3]: https://developer.android.com/training/enterprise/work-policy-ctrl.html#apply_restrictions
[1]: https://developer.android.com/training/enterprise/app-restrictions.html
[2]: https://cloud.google.com/docs/chrome-enterprise/policies
