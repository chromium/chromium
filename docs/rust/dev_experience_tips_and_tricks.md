# Tips and tricks for a nice Rust development experience

## Using VSCode

1. Ensure you're using the `rust-analyzer` extension for VSCode, rather than
   earlier forms of Rust support.
1. Run `gn` with the `--export-rust-project` flag, such as:
   `gn gen out/Release --export-rust-project`.
1. `ln -s out/Release/rust-project.json rust-project.json`
1. When you run VSCode, or any other IDE that uses
   [rust-analyzer](https://rust-analyzer.github.io/) it should detect the
   `rust-project.json` and use this to give you rich browsing, autocompletion,
   type annotations etc. for all the Rust within the Chromium codebase.
1. Point rust-analyzer to the rust toolchain in Chromium. Otherwise you will
   need to install Rustc in your system, and Chromium uses the nightly
   compiler, so you would need that to match. Add the following to
   `.vscode/settings.json` in the Chromium checkout (e.g.
   `chromium/src/.vscode/settings.json`; create the subdirectory and file if
   they do not already exist):
   ```
   {
      // The rest of the settings...

      "rust-analyzer.cargo.extraEnv": {
        "PATH": "../../third_party/rust-toolchain/bin:$PATH",
      }
   }
   ```
   This assumes you are working with an output directory like `out/Debug` which
   has two levels; adjust the number of `..` in the path according to your own
   setup.

## Suppressing unused code warnings

We don't want to commit Rust code with unused variables,
but work-in-progress changes often temporarily have unused code.
The following options exist for dealing with unused code warnings:

* Avoid warnings by prefixing
  unused variables, parameters, and other APIs
  with an underscore character
  (e.g. using `_foo` rather than `foo` as the name).
* Temporarily insert `#![allow(unused)] /* DO NOT
  SUBMIT */` at the top of the affected `.rs` files.
    - See `rustc -W help` for a list of warnings covered by the `unused` group
      of warnings.
    - You can also use `#![warn(...)]` to still see the warnings, but avoid
      treating them as errors.
* Locally set `treat_warnings_as_errors = false` in your `args.gn`.
  This will affect all Rust and Clippy warnings (as well as C/C++ warnings).

## Better Gerrit experience for 100-columns-wide code

Rust Style Guide
[allows](https://doc.rust-lang.org/style-guide/index.html?highlight=100%20characters#indentation-and-line-width)
up to 100-characters-wide lines
([just like Java](https://source.android.com/docs/setup/contribute/code-style#limit-line-length)).
If Gerrit is configured for 80-characters-wide lines, it can lead to a
suboptimal user experience.  This can be resolved by changing Gerrit settings
(e.g. at https://chromium-review.googlesource.com/settings) as follows:

* Diff Preferences => Diff width: 101
* Edit Preferences => Columns: 100
