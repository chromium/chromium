# Chromium Rust Style Guide

_For other languages, please see the [Chromium style
guides](https://chromium.googlesource.com/chromium/src/+/main/styleguide/styleguide.md)._

Chromium follows the upstream
[Rust Style Guide](https://doc.rust-lang.org/style-guide/).
The style is enforced by `git cl format` and configured by `//.rustfmt.toml`.

## FAQ

### Q: How to improve my Gerrit experience?

Rust Style Guide
[allows](https://doc.rust-lang.org/style-guide/index.html?highlight=100%20characters#indentation-and-line-width)
up to 100-characters-wide lines
([just like Java](https://source.android.com/docs/setup/contribute/code-style#limit-line-length)).
If Gerrit is configured for 80-characters-wide lines, it can lead to a
suboptimal user experience.  This can be resolved by changing Gerrit settings
(e.g. at https://chromium-review.googlesource.com/settings) as follows:

* Diff Preferences => Diff width: 101
* Edit Preferences => Columns: 100
