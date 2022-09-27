#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# DO THIS BEFORE RUNNING THE SCRIPT:
#   1. Update the ${date} variable to a MM-DD value for the first date to
#      consider for a calendar year.
#   2. Update the list of years passed to the "distribution function"

# Inputs:
#   org-list.txt (file) A map from org name to email domain (manually
#       maintained)
#  git-dirs.txt (file) A map from separate git repositories to the library name
#       (manually maintained)
## Output: stdout
#   =======================
#   YYYY - YYYY
#   org1,commit_count
#   org2,commit_count
#   ...

topdir=$(git rev-parse --show-toplevel)
scriptdir=$(cd $(dirname $0); pwd)

botpatterns="chrome-metrics-team+robot@google.com|chrome-release-bot@chromium.org|gserviceaccount.com"

cd $topdir

declare -A pattern_to_org
for o in `cat $scriptdir/org-list.txt`; do
  p=(${o//,/ })
  pattern_to_org["${p[1]}"]=${p[0]}
done

function distribution {
  echo =======================
  echo $1 - $2
  echo "Org,Commit count"
  (for repo in `cat $scriptdir/git-dirs.txt`; do
    repoline=(${repo//,/ })
    d=${repoline[0]}
    cd $d

    for pattern in $( \
      git log --after="$1-${date}" --before="$2-${date}" \
        --pretty=format:"%ae" | \
        grep -Ev ${botpatterns} | awk -F@ '{print $2}'); do
      if [ -n "${pattern_to_org["$pattern"]}" ]; then
        echo ${pattern_to_org["$pattern"]}
      else
        echo Others
      fi
    done
    cd $topdir
  done) | sort | uniq -c | sort -nr | awk '{printf("%s,%d\n",$2,$1)}'
}

# Update this to what you want!
date=01-01

# Update this to what you want!
distribution 2015 2016
distribution 2016 2017
distribution 2017 2018
distribution 2018 2019
distribution 2019 2020
distribution 2020 2021
