# How to add a new dependency on Linux

For this example, imagine we want to add a dependency on the DBus client
library. Note that this dependency already exists.

Test locally and do not start committing changes until you finish [testing that
the new dependency
works](#Test-that-you-can-build-and-run-against-the-new-dependency).

## (Debian) Determine which packages are needed

### Which dev package do we need?

The DBus documentation includes examples that include the DBus header like this:

```
#include <dbus/dbus.h>
```

Searching for `dbus/dbus.h` on
[`packages.debian.org`](https://packages.debian.org/) yields only 1 result:
[`libdbus-1-dev`](https://packages.debian.org/buster/libdbus-1-dev). This is the
dev package that we need.

### Which library package do we need?

The page for the dev package shows only two dependencies: `pkg-config` and
[`libdbus-1-3`](https://packages.debian.org/buster/libdbus-1-3). The latter is
the one we want.

Now is a good time to make sure the package is available (and that the minimum
version is available) on all supported distros. The source of truth for
supported Debian-based distros is given in the `SUPPORTED_DEBIAN_RELEASES` and
`SUPPORTED_UBUNTU_RELEASES` variables in
[`//chrome/installer/linux/debian/update_dist_package_versions.py`](https://cs.chromium.org/chromium/src/chrome/installer/linux/debian/update_dist_package_versions.py).
Check on both [`packages.debian.org`](https://packages.debian.org/) and
[`packages.ubuntu.com`](https://packages.ubuntu.com/).

## (RPM) Determine which library is needed

Look at the [list of
files](https://packages.debian.org/buster/amd64/libdbus-1-dev/filelist) provided
by the Debian dev package. There should be at least one file with a `.so`
extension. In our case, this is `libdbus-1.so`: save this for later.

If the packages were available on all supported Debian-based distros, it's
highly likely they will be available on all RPM-based ones too. But if you want
to double-check, [`rpmfind.net`](https://www.rpmfind.net/) is a good resource.
The source of truth for supported RPM-based distros is given in the
`SUPPORTED_FEDORA_RELEASES` and `SUPPORTED_OPENSUSE_LEAP_RELEASES` variables in
[`//chrome/installer/linux/rpm/update_package_provides.py`](https://cs.chromium.org/chromium/src/chrome/installer/linux/rpm/update_package_provides.py).

## Add the dev and library packages to the sysroot

From the earlier section "(Debian) Determine which packages are needed", we know
that we need `libdbus-1-dev` and `libdbus-1-3`. Add these both to the
`DEBIAN_PACKAGES` list in
[`//build/linux/sysroot_scripts/sysroot_creator.py`](https://cs.chromium.org/chromium/src/build/linux/sysroot_scripts/sysroot_creator.py).
Building and uploading the sysroot images is detailed in [Linux sysroot
images](https://chromium.googlesource.com/chromium/src.git/+/main/docs/sysroot.md).
You may need to add additional dependent libraries for your new library.

## Whitelist the new dependencies

### Debian

Add the library package to the `PACKAGE_FILTER` variable in
[`//chrome/installer/linux/debian/update_dist_package_versions.py`](https://cs.chromium.org/chromium/src/chrome/installer/linux/debian/update_dist_package_versions.py)
and run the script.

### RPM

Add the library file to the `LIBRARY_FILTER` variable in
[`//chrome/installer/linux/rpm/update_package_provides.py`](https://cs.chromium.org/chromium/src/chrome/installer/linux/rpm/update_package_provides.py)
and run the script.

## Build against the package

### Using `pkg-config`

If the dev package provides a file with a `.pc` extension, it's a good idea to
set up your build config using `pkg-config`, as this will automatically pass
include dirs to the compiler, and library files to the linker.

`libdbus-1-dev` provides `dbus-1.pc`, so we can add this to our `BUILD.gn`:

```
import("//build/config/linux/pkg_config.gni")

# "dbus" is whatever you want to name the config.
pkg_config("dbus") {
  # "dbus-1" is the name of the .pc file.
  packages = [ "dbus-1" ]
}

component("my_awesome_component") {
  deps = [ ":dbus" ]
  ...
}
```

See
[`//build/config/linux/pkg_config.gni`](https://cs.chromium.org/chromium/src/build/config/linux/pkg_config.gni)
for further details.

### Including the library directly

If the dev package doesn't provide a `.pc` file, you will need to add the build
flags manually:

```
config("dbus") {
  # libdbus-1.so is the name of the dev library.
  libs = [ "dbus-1" ]

  include_dirs = [
    "/usr/include/dbus-1.0",
    ...
  ]
}
```

## Test that you can build and run against the new dependency

For DBus, you might try:

```
#include <dbus/dbus.h>

void TestIt() {
  DBusConnection* bus = dbus_bus_get(DBUS_BUS_SESSION, nullptr);
  DCHECK(bus);
}
```

The purpose of the test is to make sure that:

1. The include path is set up properly.
2. The library can be dynamically linked at runtime.
3. The `.deb` and `.rpm` packages can build.

To test 3, make sure your `args.gn` has the following:

```
is_component_build = false  # This is required.
use_sysroot = true  # This is the default.
# is_*san = false  # This is the default.
```

Next, build `chrome/installer/linux`. If there are dependency errors, your
package may not be available on all supported distros.

## Add packages to build deps script

Add the dev package to the `dev_list` variable in
[`//build/install-build-deps.sh`](https://cs.chromium.org/chromium/src/build/install-build-deps.sh?q=install-build-deps.sh),
and add the library package to the `common_lib_list` variable in the same file.

Note that if you are removing a package from this script, be sure to add the
removed packages to the `backwards_compatible_list` variable.

## Install packages on the bots

After adding the packages to `install-build-deps.sh`, new swarming images will
be generated and rolled out to the swarming bots. However, this can take several
days. To expedite the process, the packages can be added to the Puppet config
and rolled out immediately. To do this, add the dev package, the library
package, and the i386 version of the library package to the [Puppet config
file](https://goto.google.com/ynnzy).  For DBus, this will look like:

```
  # Add packages here temporarily to roll out to the fleet as needed.
  all:
    - libdbus-1-dev
    - libdbus-1-3
    - libdbus-1-3:i386
```

## Instrumented libraries

In order for `MSAN` to work, you will likely need to add your library package to
the instrumented libraries. To do this, add the library dev package to
[`third_party/instrumented_libs/BUILD.gn`](https://cs.chromium.org/chromium/src/third_party/instrumented_libs/BUILD.gn):

```
  # This is the minimum you will need. Check other examples in this file if
  # something goes wrong.
  instrumented_library("libdbus-1-3") {
    build_method = "debian"
  }
```

Then add `:libdbus-1-3` to
`//third_party/instrumented_libs:locally_built`'s `deps`.

See [Linux Instrumented
Libraries](https://chromium.googlesource.com/chromium/src.git/+/main/docs/linux/instrumented_libraries.md)
for instructions on building and uploading the instrumented libraries.
