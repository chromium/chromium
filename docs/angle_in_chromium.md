# Hacking on ANGLE in Chromium

In DEPS, comment out the part that looks like this.

```
#  "src/third_party/angle":
#    Var("chromium_git") + "/angle/angle.git@" + Var("angle_revision"),
```

Delete or rename third\_party/angle.

(Optional) sync and make sure the third\_party/angle directory doesn't come
back. It shouldn’t because it is no longer referenced from DEPS.

```shell
gclient sync -r CURRENT_REVISION
```

Clone the ANGLE git repository.

```
> git clone https://chromium.googlesource.com/angle/angle third_party/angle
> gclient runhooks
```

To check ANGLE builds without building all of Chromium.

```shell
ninja -C out\Release libEGL.dll
```

Change files then commit locally.

Upload to Gerrit for review. You will need to have installed the git hook as
described in the "Getting started with Gerrit for ANGLE" section of the
ContributingCode doc before committing them locally.

```shell
git cl upload
```

As with subversion and Rietveld: visit the upload link for the review site,
check the diff and the commit message then add reviewer(s) and publish.

Land your changes to the upstream repository from the Gerrit web interface.

If there are upstream changes, you may need to rebase your patches and reupload
them.

```shell
git pull
git cl upload
```

# Rolling ANGLE into Chrome

To roll DEPS, make sure this is not commented out and update the hash associated
with "angle\_revision". (Your hash will be different than the one below.)

```
  "angle_revision": "0ee126c670edae8dd1822980047450a9a530c032",
```

Then sync.

```shell
gclient sync
```

Your changes should now be in third\_party/angle.
