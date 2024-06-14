#!/bin/bash -e

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SCRIPTDIR="$(dirname "$(readlink -f "$0")")"
PACKAGE="chrome-remote-desktop"
ARCHITECTURE=$(dpkg-architecture | awk -F '=' '/DEB_BUILD_ARCH=/{print $2}')

# These settings are copied from chrome/installer/linux/debian/build.sh .
BASEREPOCONFIG="dl.google.com/linux/chrome-remote-desktop/deb/ stable main"
REPOCONFIG="deb [arch=${ARCHITECTURE}] http://${BASEREPOCONFIG}"
# Allowed configs include optional HTTPS support and explicit multiarch
# platforms.
REPOCONFIGREGEX="deb (\\\\[arch=[^]]*\\\\b${ARCHITECTURE}\\\\b[^]]*\\\\]"
REPOCONFIGREGEX+="[[:space:]]*) https?://${BASEREPOCONFIG}"

source ${SCRIPTDIR}/../../../../chrome/installer/linux/common/installer.include

guess_filename() {
  VERSION_FULL=$(get_version_full)
  echo ${PACKAGE}_${VERSION_FULL}_${ARCHITECTURE}.deb
}

get_version_full() {
  src_root=${src_root:-./../../../..}
  remoting_version_path=$src_root/remoting/VERSION
  chrome_version_path=$src_root/chrome/VERSION
  version_helper=$src_root/build/util/version.py

  # TODO(lambroslambrou): Refactor to share the logic with remoting.gyp.
  version_major=$($version_helper -f $chrome_version_path \
                  -f $remoting_version_path -t "@MAJOR@")
  version_minor=$($version_helper -f $remoting_version_path \
                  -t "@REMOTING_PATCH@")
  version_build=$($version_helper -f $chrome_version_path \
                  -f $remoting_version_path -t "@BUILD@")
  version_patch=$($version_helper -f $chrome_version_path \
                  -f $remoting_version_path -t "@PATCH@")
  version_full="$version_major.$version_minor.$version_build.$version_patch"
  echo $version_full
}

usage() {
  echo "usage: $(basename $0) [-hp] [-o path] [-s path]"
  echo "-h     this help message"
  echo "-p     just print the expected DEB filename that this will build."
  echo "-s     path to the top of the src tree."
  echo "-o     output directory path."
}

while getopts ":s:o:ph" OPTNAME
do
  case $OPTNAME in
    s )
      src_root="$(readlink -f "$OPTARG")"
      ;;
    o )
      OUTPUT_PATH="$(readlink -f "$OPTARG")"
      ;;
    p )
      PRINTDEBNAME=1
      ;;
    h )
      usage
      exit 0
      ;;
    \: )
      echo "'-$OPTARG' needs an argument."
      usage
      exit 1
      ;;
    * )
      echo "invalid command-line option: $OPTARG"
      usage
      exit 1
      ;;
  esac
done
shift $(($OPTIND - 1))

# This just prints the expected package filename, then exits. It's needed so the
# gyp packaging target can track the output file, to know whether or not it
# needs to be built/rebuilt.
if [[ -n "$PRINTDEBNAME" ]]; then
  guess_filename
  exit 0
fi

# get_version_full works from ${SCRIPTDIR}
# TODO(ukai): fix get_version_full so that not need to chdir?
cd "${SCRIPTDIR}"

if [[ -z "$version_full" ]]; then
  version_full=$(get_version_full)
fi

if [[ ! "$version_full" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Error: Invalid \$version_full value: $version_full" >&2
  exit 1
fi

# TODO(ukai): Include revision information in changelog when building
# from a local git-based checkout.
revision_text=""

tmpdir="$(mktemp -p ${TMPDIR:-/tmp} -d chromium_remoting_build_deb.XXXXXX)"
trap "rm -rf -- ${tmpdir}" EXIT
# dpkg-buildpackage creates ../*.deb from ${tmpdir}/linux
mkdir -p "${tmpdir}/linux"
cp -a "${SCRIPTDIR}"/* "${tmpdir}/linux"
cd "${tmpdir}/linux"

if [[ ! "$OUTPUT_PATH" ]]; then
  OUTPUT_PATH="${SCRIPTDIR}/../../../../out/Release"
fi

echo "Building version $version_full $revision_text"

# Create a fresh debian/changelog.
export DEBEMAIL="The Chromium Authors <chromium-dev@chromium.org>"
rm -f debian/changelog
debchange --create \
  --package "$PACKAGE" \
  --newversion "$version_full" \
  --force-distribution \
  --distribution unstable \
  "New Debian package $revision_text"


CRON_SCRIPT_DIR="${OUTPUT_PATH}/remoting/installer/cron"
mkdir -p ${CRON_SCRIPT_DIR}
process_template \
    "${SCRIPTDIR}/../../../../chrome/installer/linux/common/repo.cron" \
    "${CRON_SCRIPT_DIR}/chrome-remote-desktop"

# TODO(mmoss): This is a workaround for a problem where dpkg-shlibdeps was
# resolving deps using some of our build output shlibs (i.e.
# out/Release/lib.target/libfreetype.so.6), and was then failing with:
#   dpkg-shlibdeps: error: no dependency information found for ...
# It's not clear if we ever want to look in LD_LIBRARY_PATH to resolve deps,
# but it seems that we don't currently, so this is the most expediant fix.
SAVE_LDLP=$LD_LIBRARY_PATH
unset LD_LIBRARY_PATH
BUILD_DIR=$OUTPUT_PATH SRC_DIR=${SCRIPTDIR}/../../../.. \
  dpkg-buildpackage -b -us -uc
LD_LIBRARY_PATH=$SAVE_LDLP

mv ../${PACKAGE}_*.deb "$OUTPUT_PATH"/
mv ../${PACKAGE}_*.changes "$OUTPUT_PATH"/
