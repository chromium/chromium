#!/usr/bin/env sh
set -ex

cd "${0%/*}"
virtualenv -p python .virtualenv
.virtualenv/bin/pip install pyyaml==5.4.1 cairocffi==1.3.0
.virtualenv/bin/python gentest.py
