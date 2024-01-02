#!/bin/bash

set -e
cd $(dirname $0)

# Clone androidx repo.
rm -rf temp-git-clone
git clone --depth 1 https://android.googlesource.com/platform/frameworks/support temp-git-clone

# Update sources.txt
cd temp-git-clone
find window/extensions -path "*/main/*.java" | sort > ../sources.txt
cd ..

# Copy the .java files.
rm -r java
for path in $(cat sources.txt); do
  mkdir -p java/$(dirname "$path")
  cp "temp-git-clone/$path" "java/$path"
done

rm -rf temp-git-clone

echo "Job's Done."
