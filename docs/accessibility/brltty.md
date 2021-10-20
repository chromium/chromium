# BRLTTY in Chrome OS

Chrome OS uses the open-source [BRLTTY](http://mielke.cc/brltty/)
library to provide support for refreshable braille displays.

We typically ship with a stable release build of BRLTTY plus some
permanent or cherry-picked patches.

## Updating BRLTTY or adding a patch

First, follow the public
[Chromium OS Developer Guide](http://www.chromium.org/chromium-os/developer-guide) to check out the source.
At a minimum you'll need to create a chroot.
You do not need to build everything from source.
You do need to start the devserver.

Next, flash your device to a very recent test build. Internally at Google
you can do this with the following command when the dev server is running,
where CHROMEBOOK_IP_ADDRESS is the IP address of your Chromebook already
in developer mode, and $BOARD is your Chromebook's board name.

```cros flash ssh://CHROMEBOOK_IP_ADDRESS xbuddy://remote/$BOARD/latest-dev/test```

The BRLTTY files can be found in this directory:

```third_party/chromiumos-overlay/app-accessibility/brltty```

###Major release

You'll first want to rename all files to the new major release. For example, brltty-5.6.ebuild and remove all revision symlinks (see below).

###Revision release

A revision release is the same release build of brltty, but with additional patches.

The first thing you'll need to do is edit the ebuild symlink to change the
revision number. The real file is something like brltty-5.4.ebuild,
but the revision will be something like brltty-5.4-r5.ebuild. You'll need
to increment it.

To increment it from r5 to r6, you'd do something like this:

```
rm brltty-5.4-r5.ebuild
ln -s brltty-5.4.ebuild brltty-5.4-r6.ebuild
git add brltty-5.4-r6.ebuild
```

The changes we make are all patches against a stable release of brltty.
To add a new patch, put it in the files/ directory and reference it in
brltty.bashrc

Once you're done adding patches or making other changes, flash it to your
device like this:

```
emerge-$BOARD brltty
cros deploy CHROMEBOOK_IP_ADDRESS brltty
```

After that, reboot your Chromebook and verify that brltty works.

To upload a change, use repo, something like this:

```
repo start <branch_name> .
git commit -a
  Bug: chromium:12345
  Test: Write what you tested here
repo upload .
```

Note that you shouldn't need to run cros_workon.

## Uprevving Brltty

This section outlines the process to upgrade Brltty to a major release.

### Prerequisites

First, download the latest brltty release tarball
http://mielke.cc/brltty/archive
E.g.
brltty-5.6.tar.gz

The server holding all Chrome OS source packages is Google Cloud Storage. In
order to update Brltty, you will need to first get started with GCS.
[Google-internal only](https://sites.google.com/a/google.com/chromeos/resources/engineering/releng/localmirror)

If you follow the alternative cli workflow, you should have the ability to
list the Chrome OS GCS bucket:

```gsutil ls gs://chromeos-localmirror/```

for example:
gs://chromeos-localmirror/distfiles/brltty-5.6.tar.gz
is the latest release as of writing.

It will also be handy to checkout brltty.

```Git clone http://github.com/brltty/brltty```

And follow the instructions in the readme to configure/build.

### Upload the latest stable release of brltty.

You can do this via ```gsutil cp```.

After copying, you will likely want the package to be world readable:

```
gsutil acl ch -u AllUsers:R gs://chromeos-localmirror/distfiles/brltty-5.6.tar.gz

```

### Upreving

Next, you will need to uprev the ebuild. Do this by renaming all files from the previous version to the new one.
E.g.
Brltty-5.4.ebuild -> brltty-5.6.ebuild

Start a build with your changes by doing

emerge brltty
(or emerge{$BOARD} brltty).

Note: Manifest has various checksums computed based on the release you uploaded to GCS. Each of these will need to be replaced/updated.

This should be enough to kick off a build. It is likely patches won’t apply cleanly.
Apply patches
It is often much easier to apply patches to your local checkout of brltty from github, an build there.

```git tags```

Will ensure you find the right release. You can then checkout that release via

```Git checkout tags/<tag_name>```

Finally apply each of the *.patch files to this tag of brltty.

This is more or less straightforward. If conflicts arise, it is useful to list
commits to the file containing the conflict

```git log --oneline <file>```

then understanding the history since the last release. If the patch is already
upstreamed, you can remove it from the Chrome OS repo.

### Chrome side changes

Chrome communicates with brltty using libbrlapi.
libbrlapi resides at //third_party/libbrlapi.

Chrome loads this library dynamically and hard-codes the expected version of the api so in
chrome/browser/extensions/api/braille_display_private/braille_controller_brlapi.cc

which should match up with the header in
third_party/libbrlapi/brlapi.h.

During uppreving, if brltty increments its api version, it will be necessary to update the header for libbrlapi, as well as incrementing the supported api version of the libbrlapi shared object.

First, grab the generated header from your Chrome OS build above.
cp <chromeos root>/build/$BOARD/usr/include/brlapi.h <chrome_root>/third_party/libbrlapi/brlapi.h

This header contains the specific socket path for Chrome OS which differs from brltty defaults.

Next, ensure the version in
chrome/browser/extensions/api/braille_display_private/braille_controller_brlapi.cc

matches the one in the new brltty.

#### ChromeVox
ChromeVox keeps a list of bluetooth braille display names
(search for bluetooth_display_manager.js).

Within the brltty sources (as of 6.3), one can find all bluetooth display names
in:
brltty/Programs/bluetooth_names.c

### Testing

Firstly, try to test against brltty on linux. This involves building brltty at
the proposed stable release and fully patching all of our changes from Chrome
OS.

You would do something like:

```
./autogen
./configure
make
./run-brltty -n
```

This will launch brltty (in the foreground and not as a daemon).

Any connected displays should automatically work.

Next, once you have a build deployed on a Chrome OS machine, here are a few
useful things to check:
* Routing keys within a text field
* Routing keys on a link
* Basic braille output
* Chorded commands (e.g. space + s to toggle speech)
* Typing (e.g. dots 1-2-3 within a text field)
* Display specific hardware keys
* Unload ChromeVox (ctrl+alt+z), plug in a display; ChromeVox should auto
start

Try to test with at least two displays.
###Debugging

In the event things don't go well (such as no braille appearing on the display),
you may find it helpful to:

1. examine chrome logging

/var/log/chrome

Look for any errors in *brl* related files. For example, a new release of
libbrlapi could require additional so versions be added to our loader.

2. modify the way in which brltty gets run.

In particular, look at the invocation of the minijail in

third_party/chromiumos-overlay/app-accessibility/brltty/files/brltty

You may want to add the '-l debug' flag to the brltty call and redirect stderr/stdout to a file.
... brltty -n ... -l debug,server,usb,brldrv ... > /tmp/brltty_errors 2>&1
