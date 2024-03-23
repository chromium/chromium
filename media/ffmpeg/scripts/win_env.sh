#!/bin/bash

# Copyright 2014 The Chromium Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Sets up the appropriate environment for Visual Studio 2015 command line
# development. Assumes the toolchain has been installed via depot_tools.
# The environment settings only persist while the script is executing. The
# command argument must be supplied to be run by this script while the
# environment is still configured.

if [ "$#" -lt 3 ]; then
  echo "Usage: $(basename $0) /path/to/depot_tools arch command"
  echo "    arch     must be either x86 or x64"
  echo "    command  command to execute after environment is configured"
  exit 1
fi

if [ ! -d $1 ]; then
  echo "Directory does not exist: $1"
  exit 1
fi

VSPATH=$1/win_toolchain/vs_files/1180cb75833ea365097e279efb2d5d7a42dee4b0

if [ ! -d $VSPATH ]; then
  BIW="http://www.chromium.org/developers/how-tos/build-instructions-windows"
  echo "Visual Studio 2015 toolchain not found: $VSPATH"
  echo "See $BIW"
  echo
  echo "It's also possible that we've upgraded past 2015, in which case send"
  echo "out a patch updating this script."
  exit 1
fi

function add_path {
  if [ ! -d "$1" ]; then
    echo "Cannot add '$1' to path; directory does not exist." >&2
    exit 1
  fi
  if [ -z "$path" ]; then
    path="$1"
    return
  fi
  path="$path:$1"
}

function add_include_path {
  if [ ! -d "$1" ]; then
    echo "Cannot add '$1' to include path; directory does not exist." >&2
    exit 1
  fi
  if [ -z "$include" ]; then
    include="$(cygpath -w $1)"
    return
  fi
  include="$include;$(cygpath -w $1)"
}

function add_lib_path {
  if [ ! -d "$1" ]; then
    echo "Cannot add '$1' to lib path; directory does not exist." >&2
    exit 1
  fi
  if [ -z "$lib" ]; then
    lib="$(cygpath -w $1)"
    return
  fi
  lib="$lib;$(cygpath -w $1)"
}

case "$2" in
  "x86")
    add_path $VSPATH/win_sdk/bin/x86
    add_path $VSPATH/VC/bin/amd64_x86
    add_path $VSPATH/VC/bin/amd64
    add_path $VSPATH/VC/Tools/MSVC/14.11.25503/bin/HostX86/x86

    add_lib_path $VSPATH/VC/Tools/MSVC/14.11.25503/lib/x86
    add_lib_path $VSPATH/win_sdk/Lib/10.0.15063.0/ucrt/x86
    add_lib_path $VSPATH/win_sdk/Lib/10.0.15063.0/um/x86
    add_lib_path $VSPATH/VC/Tools/MSVC/14.11.25503/atlmfc/lib/x86
    add_lib_path $VSPATH/VC/Tools/MSVC/14.11.25503/bin/HostX86/x86
    ;;

  "x64")
    add_path $VSPATH/win_sdk/bin/x64
    add_path $VSPATH/VC/bin/amd64
    add_path $VSPATH/VC/Tools/MSVC/14.11.25503/bin/HostX64/x64

    add_lib_path $VSPATH/VC/Tools/MSVC/14.11.25503/lib/x64
    add_lib_path $VSPATH/win_sdk/Lib/10.0.15063.0/ucrt/x64
    add_lib_path $VSPATH/win_sdk/Lib/10.0.15063.0/um/x64
    add_lib_path $VSPATH/VC/Tools/MSVC/14.11.25503/atlmfc/lib/x64
    add_lib_path $VSPATH/VC/Tools/MSVC/14.11.25503/bin/HostX64/x64
    ;;

  *)
    echo "Unknown architecture: $2"
    exit 1
    ;;
esac

# Common for x86 and x64.
add_path $(dirname $(readlink -f "$0")) # For cygwin-wrapper.
add_include_path $VSPATH/win_sdk/Include/10.0.15063.0/ucrt
add_include_path $VSPATH/win_sdk/Include/10.0.15063.0/um
add_include_path $VSPATH/win_sdk/Include/10.0.15063.0/shared
add_include_path $VSPATH/VC/Tools/MSVC/14.11.25503/include
add_include_path $VSPATH/VC/Tools/MSVC/14.11.25503/atlmfc/include

export PATH=$path:$PATH
export INCLUDE=$include
export LIB=$lib

# Now execute whatever is left trailing.
shift
shift
"$@"
