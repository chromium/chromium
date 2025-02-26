## Permissions Policy Guide (Previously Feature Policy)
Permissions policy is the new name for feature policy with a new HTTP header which uses
[Structured Field](https://www.rfc-editor.org/rfc/rfc9651) syntax.

### How to add a new feature to permissions policy

Permissions policy (see [spec](https://w3c.github.io/webappsec-permissions-policy/)) is a
mechanism that allows developers to selectively enable and disable various
[browser features an
APIs](https://cs.chromium.org/chromium/src/third_party/blink/public/mojom/feature_policy/feature_policy.mojom)
(e.g, "fullscreen", "usb", "web-share", etc.). A permissions policy can be defined
via a HTTP header and/or an iframe "allow" attribute.

Below is an example of a header policy (note that the header should be kept in
one line, split into multiple for clarity reasons):

    Permissions-Policy: web-share=(), geolocation=(self https://example.com), camera=*

- `web-share` is disabled for all browsing contexts;
- `geolocation` is disabled for all browsing contexts except for its own
  origin and those whose origin is "https://example.com";
- `camera` is enabled for all browsing contexts.

Below is an example of a container policy:

    <iframe allowpaymentrequest allow='web-share; fullscreen'></iframe>

OR

    <iframe allowpaymentrequest allow="web-share 'src'; fullscreen 'src'"></iframe>


- `payment` is enabled (via `allowpaymentrequest`) on all browsing contexts
 within the iframe;
- `web-share` and `fullscreen` are enabled on the origin of the URL of the
  iframe's `src` attribute.

Combined with a header policy and a container policy, [inherited
policy](https://w3c.github.io/webappsec-permissions-policy/#inherited-policy) defines the
availability of a feature.
See more details for how to [define an inherited policy for
feature](https://w3c.github.io/webappsec-permissions-policy/#define-inherited-policy)

#### Adding a new feature to permissions policy
A step-to-step guide with examples.

##### Shipping features behind a flag
If the additional feature is unshipped, or if the correct behaviour with feature
policy is undetermined, consider shipping the feature behind a runtime-enabled feature.

##### Define new feature
1. Permissions policy features are defined in
`services/network/public/cpp/permissions_policy/permissions_policy_features.json5`. Add the new feature,
placing any runtime-enabled feature or origin trial dependencies in its "depends_on" field as
described in the file's comments. This list is used to generate `permissions_policy_helper.cc`.

2. Append the new feature enum with a brief description as well in
`services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom`.
The enum must have the same name as the name field in the json5 file from step 1.
Run `tools/metrics/histograms/update_permissions_policy_enum.py`
to update enums.xml from the mojo enum.

3. Append the new feature name to the `PermissionsPolicyFeature` enum in
`third_party/blink/public/devtools_protocol/browser_protocol.pdl`.

4. Add the new feature name to `third_party/blink/web_tests/webexposed/feature-policy-features-expected.txt`
and `third_party/blink/web_tests/wpt_internal/isolated-permissions-policy/permissions_policy.https.html`.

5. Send a Pull Request to the webappsec-permissions-policy github repo
in order to propose the new permissions policy name.
See: https://github.com/w3c/webappsec-permissions-policy/blob/main/features.md

##### Integrate the feature behaviour with permissions policy
1. The most common way to check if features are enabled is `ExecutionContext::IsFeatureEnabled`.

2. Examples:
- `web-share`: `NavigatorShare::canShare()`
- `payment`: `AllowedToUsePaymentRequest()`
- `usb`: `USB::getDevices()`

##### Write web-platform-tests
To test the new feature with permissions policy, refer to
`third_party/blink/web_tests/external/wpt/permissions-policy/README.md` for
instructions on how to use the permissions policy test framework.

##### Fenced frame considerations

Reach out to `third_party/blink/renderer/core/html/fenced_frame/OWNERS` when
adding a new permissions-backed feature so they can audit it for use with the
Fenced Frames API, or feel free to add to the audit at
`content/browser/fenced_frame/PERMISSIONS_POLICIES.md` and send to
`third_party/blink/renderer/core/html/fenced_frame/OWNERS` for review.
