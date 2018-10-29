# BRLTTY in Chrome OS

Chrome OS uses the open-source [BRLTTY](http://mielke.cc/brltty/)
library to provide support for refreshable braille displays.

We typically ship with a stable release build of BRLTTY plus some
cherry-picked patches.

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
  BUG=chromium:12345
  TEST=Write what you tested here
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

Note: Manifest has various checksums computed based on the release you uploaded to GCS. Each of these will need to be replaced/updated.

This should be enough to kick off a build. It is likely patches won’t apply cleanly.
Apply patches
It is often much easier to apply patches to your local checkout of brltty from github, an build there.

```git tags```

Will ensure you find the right release. You can then checkout that release via

```Git checkout tags/<tag_name>```

### Testing

Once you have a build deployed on a machine, here are a few useful things
to check:
* Routing keys within a text field
* Routing keys on a link
* Basic braille output
* Chorded commands (e.g. space + s to toggle speech)
* Typing (e.g. dots 1-2-3 within a text field)
* Display specific hardware keys
* Unload ChromeVox (ctrl+alt+z), plug in a display; ChromeVox should auto
start

Try to test with at least two displays.
