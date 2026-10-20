# Linux Instrumented Libraries

The instrumented libraries are a collection of Chromium's dependencies built
with *SAN enabled. The MSAN libraries are required for an MSAN build to run. The
others are optional, and are currently unused.

## Building the instrumented libraries

Building the libraries requires `apt-get source`, so the build must be done in
an Ubuntu 26.04 (Resolute) environment.
`third_party/instrumented_libs/scripts/build_in_chroot.py` takes care of this.
It sets up an Ubuntu chroot under `out/` as an unprivileged user, and builds the
libraries in it, so it works on any Linux distro.

Apt is pinned to a fixed snapshot.ubuntu.com timestamp (`SNAPSHOT` in the
script), so the package sources and build dependencies are reproducible. Bump it
to pick up newer packages.

To build both the `msan-no-origins` and `msan-chained-origins` libraries:

```shell
third_party/instrumented_libs/scripts/build_in_chroot.py
```

The first run creates the chroot in `out/instrumented_libs_chroot_resolute`,
which takes a while. Later runs reuse it. If only
`third_party/instrumented_libs/resolute/scripts/install-build-deps.sh` changed,
the new packages are installed into the existing chroot. Pass `--recreate` to
start from scratch.

The libraries are built in `out/instrumented_libs_build_resolute`, and are
packaged as `msan-*-resolute-lib.tar.gz` in the root of the source tree.

### Isolation modes

The chroot is entered using one of two mechanisms, selected with `--mode`:

*   `namespace` (the default): Unprivileged user namespaces + `chroot`. This
    runs at native speed.
*   `proot`: For machines that have unprivileged user namespaces disabled. Only
    `ptrace` is required, but builds are about 3x slower.

On a 128-thread machine, creating the chroot takes about 13 minutes in
`namespace` mode and 20 minutes in `proot` mode, mostly spent downloading and
installing packages. A clean build of both sets of libraries then takes about 13
minutes in `namespace` mode and 40 minutes in `proot` mode.

### Iterating on a package

These options are useful when fixing build failures:

*   `--packages=<pkg1>,<pkg2>`: Build only these packages (GN target names in
    `third_party/instrumented_libs/resolute/BUILD.gn`), and skip packaging.
*   `--keep-going`: Continue building other packages after a failure.
*   `--no-clobber`: Keep each package's source and build tree, and reuse it on
    the next build. Failed builds always keep their tree, under
    `out/instrumented_libs_build_resolute/Instrumented-*/gen/third_party/instrumented_libs/resolute/<pkg>`.
*   `--shell`: Open a shell in the chroot.

For example:

```shell
third_party/instrumented_libs/scripts/build_in_chroot.py --keep-going --packages=nss,pango1.0
```

See `--help` for the remaining options.

## Uploading the libraries

This requires write permission on the `chromium-instrumented-libraries` GCS
bucket. File a ticket at [go/pdeiosec-bug](https://goto.google.com/pdeiosec-bug)
to request access.

```shell
upload_to_google_storage_first_class.py -b chromium-instrumented-libraries msan-chained-origins-resolute-lib.tar.gz
upload_to_google_storage_first_class.py -b chromium-instrumented-libraries msan-no-origins-resolute-lib.tar.gz
```

Each command prints an object entry. Add (or update) them in the `objects` list
in `third_party/instrumented_libs/DEPS`.

## Testing and uploading a CL

After uploading, run `gclient sync` and test out a build with `is_msan = true`
in your `args.gn`. Try running eg. `chrome` and `unit_tests` to make sure it's
working. The binaries should work natively on gLinux.

When uploading a CL, make sure to add the following in the description so that
the MSAN bot will run on the CQ:

```
CQ_INCLUDE_TRYBOTS=luci.chromium.try:linux_chromium_msan_rel_ng
```

## Cleaning up

The chroot, its package cache, and the build directories are all under `out/`:

```shell
rm -rf out/instrumented_libs_chroot_resolute* out/instrumented_libs_build_resolute
```
