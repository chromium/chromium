# Swarming merge scripts

This directory contains Swarming merge scripts. Merge scripts run to collect the
results of a swarming run of a test suite. Their basic operation is to merge
together test results from the shard runs. They can also perform other post
processing steps, such as uploading some test results to another test result
server.

There are a few merge scripts here which are simple wrappers around other
scripts. These exist just so that every merge script we know about lives in a
centralized location.

Merge scripts are documented here:

https://cs.chromium.org/search/?q=file:swarming/api.py+%22*+merge:%22&type=cs
