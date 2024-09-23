# iOS Test Runner Plugin Interface

Copyright 2022 Google Inc.

## Overview

This directory contains the protobuf, and the generated C++ and Python files. The protobuf defines
the gRPC interface for the communication between XCTest
and iOS Test Runner (defined in src/ios/build/bots/scripts) in order to support the video
recording plugin feature when running EarlGrey tests.

Note: Files ending with pb2_grpc.py and pb2.py are auto-generated and SHOULD NOT be manually edited.

We use the built-in [XCTestObservation](https://developer.apple.com/documentation/xctest/xctestobservation)
to notify our iOS Test Runner about different lifecycles of a test case through gRPC connections.
The main test lifecycles defined by Apple are:

                testBundleWillStart:
                — — — testSuiteWillStart:
                — — — — — — testCaseWillStart:
                — — — — — — testCase:didRecord:XCTIssue
                — — — — — — testCase:didRecord:XCTExpectedFailure
                — — — — — — testCaseDidFinish:
                — — — testSuite:didRecord:XCTIssue
                — — — testSuite:didRecord:XCTExpectedFailure
                — — — testSuiteDidFinish:
                testBundleDidFinish:

Googlers can find more information in the
[design doc](https://docs.google.com/document/d/1kMzdsozzIaX1Lb-7gBT2MKATxsBuJ1V8vHGkiqz-5Qs/edit?usp=sharing)

## Updating Protobuf Files

First, make your changes in the test_plugin_service.proto file. Ensure that your changes are backward
compatible. Then follow the below instructions to re-generate the code in both Python and C++

### Python Instruction

*TODO(crbug.com/40236071): instead of manually generating the python files every time,*
*add support for generating them in the grpc_library gn template so that*
*they are auto-generated during build time*

1. install grpcio-tools by running (you only need to do this once)
        `python -m pip install grpcio-tools`
2. Regenate python code by running
        `python -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. test_plugin_service.proto`

### C++ Instruction

No work needs to be done. C++ files are dynamically generated during runtime using the grpc_library template in BUILD.gn.