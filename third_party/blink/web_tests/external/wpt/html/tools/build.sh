#!/usr/bin/env sh
set -ex

cd "${0%/*}"
virtualenv -p python3 .virtualenv
.virtualenv/bin/pip install genshi
git clone https://github.com/html5lib/html5lib-python.git .virtualenv/html5lib && cd .virtualenv/html5lib || cd .virtualenv/html5lib && git pull
# Pinned commit, to avoid html5lib from changing underneath us.
git reset --hard f7cab6f019ce94a1ec0192b6ff29aaebaf10b50d
git submodule update --init --recursive
cd ../..
.virtualenv/bin/pip install -e .virtualenv/html5lib
.virtualenv/bin/python update_html5lib_tests.py
