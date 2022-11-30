#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# DO THIS BEFORE RUNNING THE SCRIPT:
#   1. Update the ${date} variable to a YYYY-MM-DD value for the first date to
#      consider for a calendar year.

date=2021-05-21

# Inputs:
#   org-list.txt (file) A map from org name to email domain (manually
#       maintained)
#   git-dirs.txt (file) A map from separate git repositories to the library name
#       (manually maintained)
# Output (intermediate): repos-out.txt (file)
#   A list of "<directory>,<org>,<# of commits>"
# Output (final): stdout
#   A list of "<org>,<repo>,<# of commits>"

topdir=$(git rev-parse --show-toplevel)
scriptdir=$(cd $(dirname $0); pwd)

cd $topdir

declare -A org_to_repos
declare -A pattern_to_org
declare -A repo_to_name

for o in `cat $scriptdir/org-list.txt`; do
  p=(${o//,/ })
  pattern_to_org["${p[1]}"]=${p[0]}
done

commitf=$scriptdir/repos-out.txt
>$commitf

for repo in `cat $scriptdir/git-dirs.txt`; do
  repoline=(${repo//,/ })
  d=${repoline[0]}
  repo_to_name["${d}"]=${repoline[1]}
  cd $d

  tmpf=$(mktemp)
  git log --after=${date} --pretty=format:"%ae" | \
    grep -Ev 'chrome-metrics-team+robot@google.com|chrome-release-bot@chromium.org|gserviceaccount.com'| awk -F@ '{print $2}' | sort > $tmpf

  for pattern in "${!pattern_to_org[@]}"; do
    o=${pattern_to_org["$pattern"]}
    count=$(grep -E $pattern $tmpf | wc -l)
    if [[ $count != 0 ]]; then
      org_to_repos["${o}"]=${org_to_repos["${o}"]}:"${d}"
      echo ${d},${o},${count} >> $commitf
    fi
  done

  rm -f $tmpf
  cd $topdir
done

echo "Org,Repo,Count"
for o in "${!org_to_repos[@]}"; do
#  echo $o
  r=${org_to_repos["${o}"]}
  repos=(${r//:/ })
  (for r in "${repos[@]}"; do
    line=$(grep -E "^$r,$o" $commitf)
    cnt=(${line//,/ })
    echo ${repo_to_name["$r"]} ${cnt[2]}
  done) | awk '{repo[$1]+=$2} END { for (r in repo) { print "'$o'," r ", " repo[r]; } }'
done
