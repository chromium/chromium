# Updating Clang format binaries

Instructions on how to update the [clang-format binaries](clang_format.md) that
come with a checkout of Chromium.

## Prerequisites

You'll also need permissions to upload to the appropriate google storage
bucket. Chromium infrastructure team members have this, and others can be
granted the permission based on need. If you need this permission, mention this
in the tracking bug.

## Fetch and upload prebuilt clang-format binaries from recent clang rolls

Recent clang rolls can be found via looking at the history of
[update.py](https://crsrc.org/c/tools/clang/scripts/update.py). You can also
use clang-format packages built in recent successful dry run attempts at
updating clang as mentioned [here](clang_sheriffing.md).

The following will, for each supported host architecture,

* Fetch the corresponding clang-format package from the specified clang roll
* Extract and copy the clang-format binary to the proper directory
* Upload the binary into a publicly accessible google storage bucket, also
  updating the deps entries in DEPS files in the local checkout of Chrome

```shell
cd $SRC/chromium/src

GS_PATH=gs://chromium-browser-clang-staging
CLANG_REV=llvmorg-15-init-234-g567890abc-2

echo Linux
gsutil cp $GS_PATH/Linux_x64/clang-format-$CLANG_REV.tgz /tmp
tar xf /tmp/clang-format-$CLANG_REV.tgz -C buildtools/linux64 --strip-component=1 bin/clang-format

echo Win
gsutil cp $GS_PATH/Win/clang-format-$CLANG_REV.tgz /tmp
tar xf /tmp/clang-format-$CLANG_REV.tgz -C buildtools/win --strip-component=1 bin/clang-format.exe

echo 'Mac x64'
gsutil cp $GS_PATH/Mac/clang-format-$CLANG_REV.tgz /tmp
tar xf /tmp/clang-format-$CLANG_REV.tgz -C buildtools/mac --strip-component=1 bin/clang-format
mv buildtools/mac/clang-format buildtools/mac/clang-format.x64

echo 'Mac arm64'
gsutil cp $GS_PATH/Mac_arm64/clang-format-$CLANG_REV.tgz /tmp
tar xf /tmp/clang-format-$CLANG_REV.tgz -C buildtools/mac --strip-component=1 bin/clang-format
mv buildtools/mac/clang-format buildtools/mac/clang-format.arm64

# TODO(crbug.com/339490714): Remove sha1 file creation once all downstream repos that
# use clang-format are migrated over to recursedeps into buildtools.
echo '(Legacy) Uploading to GCS and creating sha1 files'
upload_to_google_storage.py --bucket=chromium-clang-format buildtools/linux64/clang-format
upload_to_google_storage.py --bucket=chromium-clang-format buildtools/win/clang-format.exe
upload_to_google_storage.py --bucket=chromium-clang-format buildtools/mac/clang-format.x64
upload_to_google_storage.py --bucket=chromium-clang-format buildtools/mac/clang-format.arm64

echo 'Uploading to GCS and updating DEPS files'
# helper function to set deps entries in DEPS file using `gclient setdep`
# first argument relative DEPS path.
# second argument object_info
function set_deps {
  gclient setdep -r src/buildtools/$1@$2
  gclient setdep -r $1@$2 --deps-file=buildtools/DEPS
}
# helper function to upload content to GCS and set DEPS
# first argument: google storage path
# second argument: relative DEPS path
# This function parses out the object info outputted by upload_to_google_storage_first_class.py
# and formats it into a format that gclient setdep understands
function upload_and_set {
	object_info=$(upload_to_google_storage_first_class.py --bucket=chromium-clang-format $1 | jq -r .path.objects[0][] | sed -z 's/\n/,/g;s/,$/\n/')',clang-format'
	if [[ $2 == 'win/format' ]]; then
		object_info+='.exe'
	fi;
	set_deps $2 $object_info
}

upload_and_set buildtools/linux64/clang-format linux64-format
upload_and_set buildtools/win/clang-format.exe win-format
upload_and_set buildtools/mac/clang-format.x64 mac-format
upload_and_set buildtools/mac/clang-format.arm64 mac_arm64-format

# Clean up
rm /tmp/clang-format-$CLANG_REV.tgz
# These aren't in .gitignore because these mac per-arch paths only exist when updating clang-format.
# gclient runhooks puts these binaries at buildtools/mac/clang-format.
rm buildtools/mac/clang-format.x64 buildtools/mac/clang-format.arm64
```

## Check that the new clang-format works as expected

Compare the diffs created by running the old and new clang-format versions to
see if the new version does anything unexpected. Running them on some
substantial directory like `third_party/blink` or `base` should be sufficient.
Upload the diffs as two patchsets in a CL for easy inspection of the
clang-format differences by choosing patchset 1 as the base for the gerrit diff.

```shell
## New gerrit CL with results of old clang-format.
# use old clang-format
find base -name '*.cc' -o -name '*.c' -o -name '*.h' -o -name '*.mm' | xargs ./buildtools/linux64-format/clang-format -i
git commit -a
git cl upload --bypass-hooks
## New patchset on gerrit CL with results of new clang-format.
# update to new clang-format
find base -name '*.cc' -o -name '*.c' -o -name '*.h' -o -name '*.mm' | xargs ./buildtools/linux64/clang-format -i
git commit -a --amend --no-edit
git cl upload --bypass-hooks
```

If there are any unexpected diffs, file a bug upstream (and fix it if you can :)).

## Upload a CL according to the following template

    Update clang-format binaries and scripts for all platforms.

    I followed these instructions:
    https://chromium.googlesource.com/chromium/src/+/main/docs/updating_clang_format_binaries.md

    The binaries were built at clang revision ####### on ##CRREV##.

    Diff on base/ from previous revision of clang-format to this version:
    https://crrev.com/c/123123123/1..2

    Bug: #######

The change should **always** include new `.sha1` files for each platform (we
want to keep these in lockstep), should **never** include `clang-format`
binaries directly. The change should **always** update `README.chromium`

clang-format binaries should weigh in at 1.5MB or less. Watch out for size
regressions.

## Clean up downloaded binaries
Delete the binaries that were just extracted. To use the new binaries that were
updated in the DEPS files, run gclient sync.
```shell
rm buildtools/linux64/clang-format
rm buildtools/win/clang-format.exe
rm buildtools/mac/clang-format.x64
rm buildtools/mac/clang-format.arm64
```
