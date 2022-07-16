# Linux Instrumented Libraries

The instrumented libraries are a collection of Chromium's dependencies built
with *SAN enabled. The MSAN libraries are required for an MSAN build to run. The
others are optional, and are currently unused.

## Building the instrumented libraries

### Setting up a chroot

Building the libraries requires `apt-get source`, so the build must be done from
an Ubuntu 16.04 environment. The preferred way is using a chroot. To get
started, install `debootstrap` and `schroot`. If you're running a Debian-based
distro, run:

```shell
sudo apt install debootstrap schroot
```

Create a configuration for a Xenial chroot:

```shell
sudo $EDITOR /etc/schroot/chroot.d/xenial_amd64.conf
```

Add the following to the new file, replacing the instances of `thomasanderson`
with your own username.

```
[xenial_amd64]
description=Ubuntu 16.04 Xenial for amd64
directory=/srv/chroot/xenial_amd64
personality=linux
root-users=thomasanderson
type=directory
users=thomasanderson
```

Bootstrap the chroot:

```shell
sudo mkdir -p /srv/chroot/xenial_amd64
sudo debootstrap --variant=buildd --arch=amd64 xenial /srv/chroot/xenial_amd64 http://archive.ubuntu.com/ubuntu/
```

If your `$HOME` directory is not `/home` (as is the case on gLinux), then route
`/home` to the real thing. `schroot` automatically mounts `/home`, which is
where I'm assuming you keep your source tree and `depot_tools`.

```shell
sudo mount --bind "$HOME" /home
```

Add `sources.list`:

```shell
sudo $EDITOR /srv/chroot/xenial_amd64/etc/apt/sources.list
```

Add the following contents to the file:

```
deb     http://archive.ubuntu.com/ubuntu/ xenial          main restricted universe
deb-src	http://archive.ubuntu.com/ubuntu/ xenial          main restricted universe
deb     http://archive.ubuntu.com/ubuntu/ xenial-security main restricted universe
deb-src http://archive.ubuntu.com/ubuntu/ xenial-security main restricted universe
deb     http://archive.ubuntu.com/ubuntu/ xenial-updates  main restricted universe
deb-src http://archive.ubuntu.com/ubuntu/ xenial-updates  main restricted universe
```

Enter the chroot and install the necessary packages:

```shell
schroot -c xenial_amd64 -u root --directory /home/dev/chromium/src
apt update
apt install lsb-release sudo python pkg-config libgtk2.0-bin libdrm-dev nih-dbus-tool help2man
```

Install library packages:

```shell
third_party/instrumented_libraries/scripts/install-build-deps.sh
```

Change to a non-root user:
```shell
exit
schroot -c xenial_amd64 -u `whoami` --directory /home/dev/chromium/src
```

Add `depot_tools` to your `PATH`. For example, I have it in `~/dev/depot_tools`,
so I use:

```shell
export PATH=/home/dev/depot_tools/:$PATH
```

Now we're ready to build the libraries. A clean build takes a little over 8
minutes on a 72-thread machine.

```shell
third_party/instrumented_libraries/scripts/build_and_package.py --parallel -j $(nproc) all xenial
```

## Uploading the libraries

This requires write permission on the `chromium-instrumented-libraries` GCS
bucket. `dpranke@` can grant access.

```shell
# Exit the chroot.
exit

# Move files into place.
mv *.tgz third_party/instrumented_libraries/binaries

# Upload.
upload_to_google_storage.py -b chromium-instrumented-libraries third_party/instrumented_libraries/binaries/msan*.tgz
```

## Testing and uploading a CL

After uploading, run `gclient sync` and test out a build with `is_msan = true`
in your `args.gn`. Try running eg. `chrome` and `unit_tests` to make sure it's
working. The binaries should work natively on gLinux.

When uploading a CL, make sure to add the following in the description so that
the MSAN bot will run on the CQ:

```
CQ_INCLUDE_TRYBOTS=luci.chromium.try:linux_chromium_msan_rel_ng
```
