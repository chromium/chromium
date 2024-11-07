// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mojo} from '//resources/mojo/mojo/public/js/bindings.js';

import {ConformanceTestInterfaceCallbackRouter, PageHandlerFactory} from './validation_test_interfaces.mojom-webui.js';

// TODO(ffred): These test cases do not match their associated expectation.
// Each case should be investigated and removed from this set.
const knownFailures = new Set([
  'conformance_mthd0_incomplete_struct',
  'conformance_mthd1_misaligned_struct',
  'conformance_mthd13_good_2',
  'conformance_mthd14_uknown_non_extensible_enum_value',
  'conformance_mthd15_uknown_non_extensible_enum_array_value',
  'conformance_mthd16_uknown_non_extensible_enum_map_key',
  'conformance_mthd16_uknown_non_extensible_enum_map_value',
  'conformance_mthd17_good',
  'conformance_mthd19_exceed_recursion_limit',
  'conformance_mthd2_multiple_pointers_to_same_struct',
  'conformance_mthd2_overlapped_objects',
  'conformance_mthd2_wrong_layout_order',
  'conformance_mthd22_empty_nonextensible_enum_accepts_no_values',
  'conformance_mthd23_array_of_optionals_less_than_necessary_bytes',
  'conformance_mthd24_map_of_optionals_less_than_necessary_bytes',
  'conformance_mthd3_array_num_bytes_huge',
  'conformance_mthd3_array_num_bytes_less_than_array_header',
  'conformance_mthd3_array_num_bytes_less_than_necessary_size',
  'conformance_mthd3_misaligned_array',
  'conformance_mthd4_multiple_pointers_to_same_array',
  'conformance_mthd4_overlapped_objects',
  'conformance_mthd4_wrong_layout_order',
  'conformance_mthd5_good',
  'conformance_mthd7_unmatched_array_elements',
  'conformance_mthd7_unmatched_array_elements_nested',
  'conformance_mthd9_good',
]);

class Fixture {
  private endpoint: mojo.internal.interfaceSupport.Endpoint;

  constructor() {
    // We only need one end of the pipe
    const pipes = Mojo.createMessagePipe();
    const receiver = new ConformanceTestInterfaceCallbackRouter();
    this.endpoint =
        mojo.internal.interfaceSupport.getEndpointForReceiver(pipes.handle0);
    // Binding is necessary to set up message routing.
    receiver.$.bindHandle(this.endpoint);
  }

  // Return true if the test case passed. False otherwise.
  runTestCase(buffer: ArrayBuffer): boolean {
    const errors: Array<string> = [];

    const oldConsoleError = console.error;
    console.error = (errMsg: string) => errors.push(errMsg);

    // Some mojo validations are done through console.assert and console.error.
    // We need to capture errors on those channels to check whether or not
    // incorrect buffers are correctly identified.
    const oldConsoleAssert = console.assert;
    console.assert = (condition: boolean, errMsg: string) => {
      if (!condition) {
        errors.push(errMsg);
      }
    };

    // Other places use exception throwing to signal errors.
    try {
      mojo.internal.interfaceSupport.acceptBufferForTesting(
          this.endpoint, buffer);
    } catch (e) {
      errors.push('' + e);
    }

    console.error = oldConsoleError;
    console.assert = oldConsoleAssert;

    return errors.length === 0;
  }
}

async function runTest(): Promise<boolean> {
  const remote = PageHandlerFactory.getRemote();

  const testCases =
      (await remote.getTestCases())
          .testCases.sort((a, b) => a.testName.localeCompare(b.testName));


  const failures = [];
  const expectedFailures = [];
  for (const testCase of testCases) {
    const shouldSucceed = testCase.expectation === 'PASS';

    const succeeded =
        (new Fixture()).runTestCase(new Uint8Array(testCase.data).buffer);

    if (succeeded !== shouldSucceed) {
      if (knownFailures.has(testCase.testName)) {
        expectedFailures.push(testCase.testName);
      } else {
        failures.push(testCase.testName);
      }
    } else {
      if (knownFailures.has(testCase.testName)) {
        throw new Error(
            testCase.testName +
            ' is now passing, please remove from known failures');
      }
    }
  }
  if (expectedFailures.length > 0) {
    console.log('tests continuing to fail: \n' + expectedFailures.join('\n'));
  }

  if (failures.length > 0) {
    throw new Error('failed the following tests: \n' + failures.join('\n'));
  }
  return Promise.resolve(true);
}

// Exporting on |window| since this method is directly referenced by C++.
Object.assign(window, {runTest});
