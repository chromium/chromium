# Linux Instrumented Libraries

The instrumented libraries are a collection of Chromium's dependencies built
with *SAN enabled. The MSAN libraries are required for an MSAN build to run. The
others are optional, and are currently unused.

## Building the instrumented libraries

### Setting up a chroot

Building the libraries requires `apt-get source`, so the build must be done from
an Ubuntu 24.04 environment. The preferred way is using a chroot. To get
started, install `debootstrap` and `schroot`. If you're running a Debian-based
distro, run:

```shell
sudo apt install debootstrap schroot
```

Create a configuration for a Noble chroot:

```shell
cat | sudo tee /etc/schroot/chroot.d/noble_amd64.conf > /dev/null <<EOF
[noble_amd64]
description=Ubuntu 24.04 Noble for amd64
directory=/srv/chroot/noble_amd64
personality=linux
root-users=$USER
type=directory
users=$USER
EOF
```

Bootstrap the chroot:

```shell
sudo mkdir -p /srv/chroot/noble_amd64
sudo debootstrap --variant=buildd --arch=amd64 noble /srv/chroot/noble_amd64 http://archive.ubuntu.com/ubuntu/
```

If your `$HOME` directory is not `/home` (as is the case on gLinux), then route
`/home` to the real thing. `schroot` automatically mounts `/home`, which is
where I'm assuming you keep your source tree and `depot_tools`.

```shell
sudo mount --bind "$HOME" /home
```

Populate `sources.list`:

```shell
cat | sudo tee -a /srv/chroot/noble_amd64/etc/apt/sources.list > /dev/null <<EOF
deb     http://archive.ubuntu.com/ubuntu/ noble          main restricted universe
deb-src	http://archive.ubuntu.com/ubuntu/ noble          main restricted universe
deb     http://archive.ubuntu.com/ubuntu/ noble-security main restricted universe
deb-src http://archive.ubuntu.com/ubuntu/ noble-security main restricted universe
deb     http://archive.ubuntu.com/ubuntu/ noble-updates  main restricted universe
deb-src http://archive.ubuntu.com/ubuntu/ noble-updates  main restricted universe
EOF
```

Enter the chroot and install the necessary packages:

```shell
schroot -c noble_amd64 -u root --directory /home/dev/chromium/src
apt update
apt upgrade
apt install lsb-release sudo python-is-python3 pkg-config libgtk2.0-bin libdrm-dev help2man git fakeroot gyp patchelf
```

Install library packages:

```shell
third_party/instrumented_libs/noble/scripts/install-build-deps.sh
```

Change to a non-root user:
```shell
exit
schroot -c noble_amd64 -u `whoami` --directory /home/dev/chromium/src
```

On your host, mount `/dev/shm/`.  Replace `*` with the actual path if you have multiple chroots.
```shell
sudo mount --bind /dev/shm /run/schroot/mount/noble_amd64-*/dev/shm
```

Add `depot_tools` to your `PATH`. For example, I have it in `~/dev/depot_tools`,
so I use:

```shell
export PATH=/home/dev/depot_tools/:$PATH
```

Now we're ready to build the libraries. A clean build takes a little over 8
minutes on a 72-thread machine.

```shell
third_party/instrumented_libs/scripts/build_and_package.py --parallel -j $(nproc) all noble
```

## Uploading the libraries

This requires write permission on the `chromium-instrumented-libraries` GCS
bucket. File a ticket at [go/peepsec-bug](https://goto.google.com/peepsec-bug)
to request access.

```shell
# Exit the chroot.
exit

# Move files into place.
mv *.tgz third_party/instrumented_libs/binaries

# Upload.
upload_to_google_storage.py -b chromium-instrumented-libraries third_party/instrumented_libs/binaries/msan*.tgz
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

## Cleaning up chroots

This can be useful for restarting from scratch with a new chroot, e.g. to
validate the build instructions above.

```shell
sudo rm /etc/schroot/chroot.d/noble_amd64.conf
sudo rm -rf /srv/chroot/noble_amd64
```

If `rm` complains about active mount points, list the active chroot session(s):
```shell
schroot --list --all-sessions
```
Which should print something like:
```
session:noble_amd64-714a3c01-9dbf-4c98-81c1-90ab8c4c61fe
```

Then shutdown the chroot session with:
```shell
schroot -e -c noble_amd64-714a3c01-9dbf-4c98-81c1-90ab8c4c61fe
```
