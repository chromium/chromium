#!/bin/bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#######################################################################
# See go/clank-autoroll#androidx-bisect for instructions.
#######################################################################

function abort() {
  echo "$1" >&2
  # 255 will abort the bisect. 125 marks commit as "unknown".
  exit ${2-255}
}

if [[ -z "$PERF_BENCHMARK" || -z "$PERF_BOT" || -z "$PERF_STORY" || -z "$PERF_METRIC" ]]; then
  abort 'Example Usage:
PERF_BENCHMARK="startup.mobile" \
PERF_BOT="android-pixel4-perf" \
PERF_STORY="cct_coldish_bbc" \
PERF_METRIC="messageloop_start_time" $0 "$@"
fi

# Determine Chromium src path based on script location.
CHROMIUM_SRC="$(dirname $0)/../.."
# Determine Android src path based on current directory (git bisect must be run from it).
# Allow running from superproject root, or from within frameworks/support.
if [[ -d frameworks/support ]]; then
  SUPERPROJECT_DIR="$(pwd)"
elif [[ -d ../../frameworks/support ]]; then
  SUPERPROJECT_DIR="$(pwd)/../.."
else
  abort "Expected to have been run from androidx checkout."
fi
FRAMEWORKS_SUPPORT_DIR="$SUPERPROJECT_DIR/frameworks/support"

set -x
set -o pipefail

cd "$CHROMIUM_SRC"
# Sanity checks:
pinpoint auth-info >/dev/null || abort "First run: pinpoint auth-login"
cipd auth-info >/dev/null || abort "First run: cipd auth-login"
# Needed for androidx sync.
gcertstatus -check_ssh=false || abort "First run: gcert"

# Allow //third_party/androidx/cipd.yaml to be listed.
local changes=$(git status --porcelain | grep -v cipd.yaml)
[[ -n "$changes" ]] && abort "git status reports changes present."

# Ensure we're on a non-main branch.
git_branch=$(git rev-parse --abbrev-ref HEAD)
[[ "$git_branch" = HEAD ]] && abort "Need to be on a branch"
[[ "$git_branch" = main ]] && abort "Need to be on a non-main branch"

# Use the most recent non-local commit as the diffbase.
git_base_rev=$(git merge-base origin/main HEAD)

cd "$SUPERPROJECT_DIR"
git submodule update --recursive --init || abort "AndroidX sync Failed"
cd "$FRAMEWORKS_SUPPORT_DIR"
# Creates a local maven repo in: out/dist/repository. Aborts bisect upon failure.

SNAPSHOT=true ./gradlew createArchive || abort "AndroidX Build Failed" 125
cd $CHROMIUM_SRC
third_party/androidx/fetch_all_androidx.py \
    --local-repo "$SUPERPROJECT_DIR/out/dist/repository" || abort "fetch_all_androidx.py failure"

super_rev=$(git -C "$SUPERPROJECT_DIR" rev-parse HEAD)
support_rev=$(git -C "$FRAMEWORKS_SUPPORT_DIR" rev-parse HEAD)
cipd_output=$(cipd create -pkg-def third_party/androidx/cipd.yaml \
  -tag super_rev:$super_rev \
  -tag support_rev:$support_rev | grep Instance:) || abort "cipd failure"
# E.g.: Instance: experimental/google.com/agrieve/androidx:3iiAIwUqY5nB5O9ArpioN...
cipd_output=${cipd_output#*Instance: }
cipd_package=${cipd_output%:*}  # Strip after colon.
cipd_package_escaped=${cipd_package//\//\\/}
cipd_instance=${cipd_output#*:}  # Strip before colon.
# This needs to be run only once per package, but it's simpler to run it every time.
cipd acl-edit "$cipd_package" -reader group:all || abort "cipd acl failure"
# gclient setdep does not allow changing CIPD package, so perl it is.
perl -0777 -i -pe "s/(.*src\/third_party\/androidx.*?packages.*?package': ')(.*?)('.*?version': ')(.*?)('.*)/\${1}${cipd_package_escaped}\${3}${cipd_instance}\$5/s" DEPS

git add DEPS || abort "git add failed"
git commit -m "androidx bisect super_rev=${super_rev::9} support_rev=${support_rev::9}"
EDITOR=true git cl upload --bypass-hooks --bypass-watchlists --no-autocc --message "androidx bisect review" \
    --title "super_rev=${super_rev::9} support_rev=${support_rev::9}" || abort "Upload CL failed"
review_number=$(git cl issue | grep -P --only-matching '(?<=Issue number: )(\d+)') || abort "parsing issue failed"

review_url="https://chromium-review.googlesource.com/c/chromium/src/+/$review_number"
# Returns non-zero if metric changed or if command fails.
pinpoint_job=$(pinpoint experiment-telemetry-start \
  -base-commit $git_base_rev -exp-commit $git_base_rev \
  -benchmark $PERF_BENCHMARK -cfg $PERF_BOT \
  -story $PERF_STORY -measurement $PERF_METRIC \
  -check-experiment -wait -quiet \
  -exp-patch-url "$review_url" \
  | tee /dev/stderr \
  | grep -P --only-matching '(?<=/job/)\S+')
# E.g.: Pinpoint job scheduled: https://pinpoint-dot-chromeperf.appspot.com/job/13af94f2ea0000
retcode=$?

if [[ -z "$pinpoint_job" ]]; then
  abort "Failed to parse pinpoint job"
fi
# E.g.: state:  SUCCEEDED
if ! pinpoint get-job -name $pinpoint_job | grep -q "state.*SUCCEEDED"; then
  abort "Pinpoint did not finish successfully."
fi

# Expect a significant difference for good changes.
if [[ $retcode = 0 ]]; then
  exit 1
fi
exit 0
