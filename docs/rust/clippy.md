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

## Known issues

* https://crbug.com/472338477: Can't invoke Clippy on a proc-macro crate
* https://github.com/rust-lang/rust-clippy/issues/16317:
  `clippy::missing_safety_doc` triggers on `cxx::bridge`-generated code
  ([Example `#[allow(...)]` used as a workaround](https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/rust/sequences/cxx.rs;l=24-25;drc=3abf739f4b711e4339c793af95ab482ca3a6c7fc))

## Future work

Next steps:

* Experiment with tooling for automatically applying
  fixes suggested by Clippy (and/or `rustc`)
* Add a very brief section documenting the Chromium policy for Clippy.
  Initial draft: https://docs.google.com/document/d/1S3gs-PV_lrS6Sshw7x-We0WkQN28cskL02F9ZS6L6oo/edit?usp=sharing
* Enable Clippy on some CQ bots + do a broader announcement
* Figure out how Chromium-specific lints can be added
* Consider opting into additional lints.  Examples:
    - [`undocumented_unsafe_blocks`](https://rust-lang.github.io/rust-clippy/master/index.html#undocumented_unsafe_blocks)
    - [`multiple_unsafe_ops_per_block`](https://rust-lang.github.io/rust-clippy/master/index.html?groups=restriction#multiple_unsafe_ops_per_block)

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
