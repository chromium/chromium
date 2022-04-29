#!/usr/bin/env sh
set -ex

cd "${0%/*}"
virtualenv -p python3 .virtualenv
.virtualenv/bin/pip install -r ./requirements.txt
cd ..
tools/.virtualenv/bin/python ./tools/generate.py ./tools/fetch-metadata.conf.yml
