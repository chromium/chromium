# iOS Test Runner Plugin Server

Copyright 2022 Google Inc.

## Overview

This directory contains the server side code for running test plugins throughout different
lifecycle stages of a XCTest case. Currently only video recording plugin is supported.
Googlers can find more information in the
[design doc](https://docs.google.com/document/d/1kMzdsozzIaX1Lb-7gBT2MKATxsBuJ1V8vHGkiqz-5Qs/edit?usp=sharing)
*TODO(crbug.com/40233737): add more info about how plugin works once video plugin is implemented*

## How to Test Locally
1. Edit the comment section in test_plugin_client.py with your custom code that you want to test
2. In one terminal, start the server with
        `python test_plugin_server.py`
3. In another terminal, start the client with
        `python test_plugin_client.py`

## Unit Test
unit test should be automatically run during presubmit. You can also run it manually with
        `python test_plugin_service.py`