## Permissions Policy Guide (Previously Feature Policy)

The Permissions Policy Guide has been moved to
[services/network/public/cpp/permissions\_policy/README.md](https://crsrc.org/c/services/network/public/cpp/permissions_policy/README.md)
<!-- TODO(crbug.com/40126948): Consider moving Document Policy out of Blink as
well, at which point the two guides could be merged. -->


## Document Policy Guide
### How to add a new feature to document policy

Document Policy (see [spec](https://wicg.github.io/document-policy/)) is a
similar mechanism to Permissions Policy. It is intended to cover those kinds of
features which don't involve delegation of permission to trusted origins;
features which are more about configuring a document, or removing features
(sandboxing) from a document or a frame. Document Policy can only be set through
an HTTP header, and will not inherit to subframes.

Example HTTP header: `Document-Policy: force-load-at-top=?0, lossy-images-max-bpp=1.0`

- `force-load-at-top` is set to boolean value false (`?0` in [Structured Field
syntax](https://www.rfc-editor.org/rfc/rfc9651#section-3.3.6)), i.e. the feature
is disallowed in current document;
- `lossy-images-max-bpp` is set to 1.0, i.e. lossy image format (e.g. jpeg)
images with byte per pixel rate higher than 1.0 will be blocked.


#### Adding a new feature to document policy

##### Shipping features behind a flag
If the additional feature is unshipped, or if the correct behaviour with document
policy is undetermined, consider shipping the feature behind a runtime-enabled feature.

##### Define new feature
1. Document policy features are defined in
`third_party/blink/renderer/core/permissions_policy/document_policy_features.json5`. Add the new feature,
placing any runtime-enabled feature or origin trial dependencies in its "depends_on" field as
described in the file's comments.

2. Append the new feature enum with a brief description as well in
`third_party/blink/public/mojom/permissions_policy/document_policy_feature.mojom`

##### Integrate the feature behaviour with document policy
The most common way to check if features are enabled is `ExecutionContext::IsFeatureEnabled`.

##### Write web-platform-tests
Please add new tests to `third_party/blink/web_tests/external/wpt/document-policy/`.

#### Contacts
For more questions, please feel free to reach out to:
iclelland@chromium.org
(Emerita: loonybear@)
