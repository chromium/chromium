# Clippy in Chromium

## How to run Clippy lints against Rust code in Chromium

To opt into running Clippy, please set `enable_rust_clippy = true` in `args.gn`.
With this `args.gn` setting building a 1st-party Rust library will also
invoke Clippy.

Clippy is disabled by default to avoid adding build overhead to
Chromium engineers primarily working with C, C++, or Java.

> TODO(https://crbug.com/41484295): Another, _temporary_ reason for disabling
> Clippy by default, is that we don't yet enforce Clippy-cleaniness and
> therefore Chromium is not yet Clippy-clean.
> Once Clippy is enabled on some CQ bots, then we should:
>
> - Delete this snippet that talks about "temporary reason"
> - Mention CQ coverage (the fact that it exists + which bots).
> - Encourage everyone working with Rust sources to "shift left"
>   and set `enable_rust_clippy = true`.

## Chromium policy

Chromium has a relaxed, non-strict Clippy policy.
Clippy is a tool that hopefully helps to improve the quality of Chromium code,
but ultimately we trust the judgement of Chromium engineers.

* We hope that *most* Clippy violations
  point out actual opportunities for improving the code
  (e.g. catching common mistakes, teaching idiomatic Rust patterns, etc.).
* If a Clippy lint doesn’t seem useful in a certain scenario,
  then use your best judgement
  and feel free to suppress the lint using the `#[allow(...)]` macro.
    - We recommend leaving a short comment
      that will help future readers understand
      why the lint was suppressed.
    - We recommend consulting with the
      [@security team](https://groups.google.com/u/1/a/chromium.org/g/security)
      before suppressing `unsafe`-related lints like
      [`clippy::missing_safety_doc`](https://rust-lang.github.io/rust-clippy/master/index.html?search=safe#missing_safety_doc)
      or
      [`clippy::clippy::undocumented_unsafe_blocks`](https://rust-lang.github.io/rust-clippy/master/index.html?search=safe#undocumented_unsafe_blocks).
    - We recommend applying suppression to a single item,
      but it is okay to suppress at a module, or crate level if it makes sense
      (e.g. if all functions in a given test module
      have to violate a certain lint)
* If a Clippy lint is frequently suppressed, then
  one option on the table is
  disabling the lint globally in Chromium - please talk with
  [@chrome-rust](https://groups.google.com/a/google.com/g/chrome-rust)
  if you find yourself oftentimes disabling a particular lint.

For a list of Clippy lints that apply by default to Chromium targets please
see the `//build/config/compiler:default_clippy_lints` target.

## Known issues

* https://crbug.com/472338477: Can't invoke Clippy on a proc-macro crate
* https://github.com/rust-lang/rust-clippy/issues/16317:
  `clippy::missing_safety_doc` triggers on `cxx::bridge`-generated code
  ([Example `#[allow(...)]` used as a workaround](https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/rust/sequences/cxx.rs;l=24-25;drc=3abf739f4b711e4339c793af95ab482ca3a6c7fc))

## Future work

Next steps:

* Fix preexisting Clippy violations: https://crbug.com/472355480
* Enable Clippy on `linux-rel-compilator` (CI + CQ)
  for initial enforcement
* Enable Clippy on `android-arm64-rel-compilator` (CI + CQ)
  to cover `ui/android/texture_compressor`
* Consider enabling Clippy on
  `ios-simulator-compilator`, `linux-chromeos-rel-compilator`,
  `mac-rel-compilator`, and `win-rel-compilator` (CI + CQ)
  to cover remaining target platforms in their basic configurations
* Figure out how Chromium-specific lints can be added
* Consider opting into additional lints.  Examples:
    - [`undocumented_unsafe_blocks`](https://rust-lang.github.io/rust-clippy/master/index.html#undocumented_unsafe_blocks)
    - [`multiple_unsafe_ops_per_block`](https://rust-lang.github.io/rust-clippy/master/index.html?groups=restriction#multiple_unsafe_ops_per_block)

## Automatically applying fix suggestions

### IDE integration

TODO: See if/how this works + write this section.


### Command-line

When build fails because of Clippy, then
a note toward the end spells out a command
for using `build/rust/apply_fixes.py`
to apply machine-applicable fix suggestions
from this particular Clippy invocation.

Example:

```
$ autoninja -C out/rel ...
...
error: this expression creates a reference which is immediately dereferenced by the compiler
   --> ../../third_party/blink/renderer/core/xml/parser/xml_ffi.rs:271:33
    |
271 |                 q_name.push_str(&pref);
    |                                 ^^^^^ help: change this to: `pref`
    |
    = help: for further information visit https://rust-lang.github.io/rust-clippy/master/index.html#needless_borrow
    = note: `-D clippy::needless-borrow` implied by `-D clippy::style`
    = help: to override `-D clippy::style` add `#[allow(clippy::needless_borrow)]`

error: aborting due to 1 previous error

NOTE: See `//docs/rust/clippy.md` for more Clippy info.
NOTE: To apply machine-applicable fix suggestions (if any) run: build/rust/apply_fixes.py out/rel clippy-driver obj/third_party/blink/renderer/core/xml_ffi.rustflags.json
...

$ build/rust/apply_fixes.py out/rel clippy-driver obj/third_party/blink/renderer/core/xml_ffi.rustflags.json
Invoking clippy-driver to repro the build problem...
Looking for machine-applicable fixes...
Applying fixes to `../../third_party/blink/renderer/core/xml/parser/xml_ffi.rs`...
  Applying a fix on line 271...

$ git diff
diff --git a/third_party/blink/renderer/core/xml/parser/xml_ffi.rs b/third_party/blink/renderer/core/xml/parser/xml_ffi.rs
index 707c74bf87784..b02eaf1266d2c 100644
--- a/third_party/blink/renderer/core/xml/parser/xml_ffi.rs
+++ b/third_party/blink/renderer/core/xml/parser/xml_ffi.rs
@@ -268,7 +268,7 @@ fn attributes_next<'a>(
         match &name.prefix {
             Some(pref) => {
                 let mut q_name = String::with_capacity(pref.len() + 1 + name.local_name.len());
-                q_name.push_str(&pref);
+                q_name.push_str(pref);
                 q_name.push(':');
                 q_name.push_str(&name.local_name);
                 attribute_view.as_mut().Populate(
```

## Implementation detail: When exactly does Clippy run?

When `enable_rust_clippy = true` then by default each Rust target
(e.g. `rust_static_library("foo")`) will have a
[`validations`](https://gn.googlesource.com/gn/+/main/docs/reference.md#var_validations)
dependency edge to an `action("foo_clippy")` target.
This means that `foo_clippy` will be invoked whenever `foo` is built (when
`ninja` is asked to build `foo` directly or when `foo` is transitively needed).
OTOH, targets that depend on `foo` will not be blocked waiting for completion
of `foo_clippy`.

If you want to minimize overhead of Clippy even further, you can consider
setting `enable_rust_clippy_eager = false`.  This removes the `validations`
dependency edge described above, so that building `foo` no longer triggers
`foo_clippy`.  This mode is discouraged, because it means that Clippy failures
will go undetected until a full build (e.g. until CQ).  Nevertheless, this mode
may be useful when wanting to quickly iterate on code.

## References

* Tracking bug: https://crbug.com/41484295/dependencies
* Initial design doc:
  ["Clippy in Chromium"](https://docs.google.com/document/d/1waxby3UnWPhU_CjKFQlQGUTMmkb0qTZK2gYwqo4KPd0/edit?usp=sharing)
* Technical deep dive:
  ["Clippy in Chromium: dep edge"](https://docs.google.com/document/d/1Ki9gwPv43p4G65cYoaPOdv3GIqHNpYHAPsXNp0Zdmjk/edit?usp=sharing)
