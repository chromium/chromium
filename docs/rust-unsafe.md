# `unsafe` Rust Guidelines

## Code Review Policy {#code-review-policy}

All `unsafe` Rust code in Chromium needs to be reviewed and LGTM-ed by a
reviewer from `//third_party/rust/UNSAFE_RUST_OWNERS`.
This policy applies to both third-party code
(e.g. under `//third_party/rust`) and first-party code.

### How to request a review

To facilitate a code review please:

* For each new or modified `unsafe` block, function, `impl`, etc.,
  add an unresolved "TODO: `unsafe` review" comment in Gerrit.
  You can consider using `tools/crates/create_draft_comments.py` to streamline
  creating such comments.

* Add `chrome-unsafe-rust-reviews@google.com` as a reviewer.

### Scope of review

Note that changes _anywhere_ in a crate that uses `unsafe` blocks may violate
the internal invariants on which those `unsafe` blocks rely. It is unrealistic
to require an `unsafe` review to re-audit all the
`unsafe` blocks each time a crate is updated, but the crate `OWNERS` and other
reviewers should be on the lookout for code changes which feel as though they
could affect invariants on which `unsafe` blocks rely.

### `OWNERS` files guidance

To require `unsafe` review for certain `.rs` files
(e.g. ones that use `unsafe` Rust)
you can forward from the file's `OWNERS` to
`//third_party/rust/UNSAFE_RUST_OWNERS`
(see comments in the latter for more details).

### Soft SLA

For incremental changes (including updating a minor version of a crate under
`//third_party/rust/chromium_crates_io`) the usual [Chromium responsiveness
expectations](cl_respect.md#expect-responsiveness) apply. (i.e. You should expect
reviewer input within 1 business day.)

For bulk changes (e.g. importing a new crate and its transitive dependencies)
the turnaround time may be longer.  This depends mostly on the amount of
`unsafe` code.  To streamline reviews and future maintainability, we ask you
kindly to prefer crates that do *not* use `unsafe` Rust code.

### Other notes

Bugs that track streamlining application of this policy are tracked under
the umbrella of https://crbug.com/393394872/dependencies.

## `cargo vet` Policy {#cargo-vet-policy}

Crates in `//third_party/rust/chromium_crates_io` need to be covered by `cargo
vet` audits.  In other words, `tools/crates/run_cargo_vet.py check` should
always succeed (this is enforced by `//third_party/rust/PRESUBMIT.py`).

### Audit criteria required for most crates

Audit criteria required for a given crate depend on how the crate is used.  The
criteria are written to
`third_party/rust/chromium_crates_io/supply-chain/config.toml` by
`tools/crates/run_gnrt.py vendor` based on whether
`third_party/rust/chromium_crates_io/gnrt_config.toml` declares that the crate
is meant to be used (maybe transitively) in a `safe`, `sandbox`, or `test`
environment.  For example, to declare that a crate is `safe` to be used in the
browser process, it needs to be audited and certified to be `safe-to-deploy`,
`ub-risk-2` or lower, and either `does-not-implement-crypto` or `crypto-safe`.

Note that some audits can be done by any engineer ("ub-risk-0" and
"safe-to-run") while others will require specialists from the
`unsafe-rust-in-chrome@google.com` group (see the ["Code Review Policy"
above](#code-review-policy).  More details about audit criteria and the required
expertise are explained in the
[auditing_standards.md](https://github.com/google/rust-crate-audits/blob/main/auditing_standards.md),
which also provides guidance for conducting delta audits.

### Some crates don't require an audit

Chromium implicitly trusts certain crate publishers.  Currently
there are two scenarios where such trust relationship may be established:

* Trusting crates authored and maintained under https://github.com/rust-lang/
  (e.g. `libc`, `hashbrown`), because they are closely related to the Rust
  toolchain (i.e. the same group managed and publishes `rustc`,
  `rustfmt`, `cargo`, `rustup`, etc.).
* Trusting crates that are part of an OS SDK (e.g. `windows-...` crates).

Chromium uses both our own audits
(stored in `third_party/rust/chromium_crates_io/supply-chain/audits.toml`)
as well as audits imported from other parts of Google
(e.g. Android, Fuchsia, etc.).  This means that adding a new crate does not
necessarily require a new audit if the crate has already been audited by
other projects (in this case, `cargo vet` will record the imported audit
in the `third_party/rust/chromium_crates_io/supply-chain/imports.lock` file).

### How to run `cargo vet` in Chromium

See
[Cargo Vet documentation](https://mozilla.github.io/cargo-vet/recording-audits.html)
for how to record the audit in `audits.toml`.
The `tools/crates/run_cargo_vet.py` may be used to invoke Chromium's copy of
`cargo-vet`.

