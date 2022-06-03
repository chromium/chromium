# Linux sysroot images

The chromium build system for Linux will (by default) use a sysroot image
rather than building against the libraries installed on the host system.
This serves several purposes.  Firstly, it ensures that binaries will run on all
supported linux systems independent of the packages installed on the build
machine.  Secondly, it makes the build more hermetic, preventing issues that
arise for variations among developers' systems.

The sysroot consists of a minimal installation of Debian/stable (or old-stable)
to ensure maximum compatibility.  Pre-built sysroot images are stored in
Google Cloud Storage and downloaded during `gclient runhooks`

## Installing the sysroot images

Installation of the sysroot is performed by
`build/linux/sysroot_scripts/install-sysroot.py`.

This script can be run manually but is normally run as part of gclient
hooks. When run from hooks this script in a no-op on non-linux platforms.

## Rebuilding the sysroot image

The pre-built sysroot images occasionally needs to be rebuilt.  For example,
when security updates to Debian are released, or when a new package is needed by
the chromium build.  If you just want to update the sysroots without adding any
new packages, skip to `Using build_and_upload.py`.

### Adding new packages

To add a new package, edit the `sysroot-creator-*.sh` scripts and modify the
`DEBIAN_PACKAGES` list.

### Rebuilding

To rebuild the images (without any changes) run the following commands:

    $ cd build/linux/sysroot_scripts
    $ ./sysroot-creator-stretch.sh BuildSysrootAll

The above command will rebuild the sysroot for all architectures. To build
just one architecture use `BuildSysroot<arch>`.  Run the script with no
arguments for a list of possible architectures.  For example:

    $ ./sysroot-creator-stretch.sh BuildSysrootAmd64

This command on its own should be a no-op and produce an image identical to
the one on Google Cloud Storage.

### Uploading new images

To upload images to Google Cloud Storage run the following command:

    $ ./sysroot-creator-stretch.sh UploadSysrootAll

Here you should use the SHA1 of the git revision at which the images were
created.

Uploading new images to Google Clound Storage requires write permission on the
`chrome-linux-sysroot` bucket.

### Rolling the sysroot version used by chromium

Once new images have been uploaded, the `sysroots.json` file needs to be updated
to reference the new versions.  This process is manual and involves updating the
`Revision` and `Sha1Sum` values in the file.

### Using `build-and-upload.py`

The `build_and_upload.py` script automates the above four steps.  It is
recommended to use this just before you're ready to submit your CL, after you've
already tested one of the updated sysroots on your local configuration.  Build
or upload failures will not produce detailed output, but will list the script
and arguments that caused the failure.  To debug this, you must run the failing
command manually.  This script requires Google Cloud Storage write permission on
the `chrome-linux-sysroot` bucket.
