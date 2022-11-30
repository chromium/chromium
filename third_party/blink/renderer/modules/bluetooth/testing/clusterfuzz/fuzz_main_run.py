# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to generate Web Bluetooth web tests that can be run in ClusterFuzz.

This script uses templates in the templates/ directory to generate html files
that can be run as web tests. The script reads a template, includes files
necessary to run as a web test, fuzzes its parameters and saves the result in
a new file in the directory specified when running the script.
"""

import argparse
import glob
import os
import sys
import tempfile
import time

from fuzzer_helpers import FillInParameter
import parameter_fuzzer
import test_case_fuzzer

JS_FILES_AND_PARAMETERS = (('testharness.js', 'INCLUDE_TESTHARNESS'),
                           ('testharnessreport.js', 'INCLUDE_REPORT'),
                           ('bluetooth-test.js', 'INCLUDE_BLUETOOTH_TEST'),
                           ('bluetooth-fake-devices.js',
                            'INCLUDE_BLUETOOTH_FAKE_DEVICES'))

SCRIPT_PREFIX = '<script type="text/javascript">\n'
SCRIPT_SUFFIX = '\n</script>\n'


def _GetArguments():
    """Parses the arguments passed when running this script.

    Returns:
      An argparse.Namespace object containing the arguments in sys.argv.
    """
    parser = argparse.ArgumentParser()

    # Arguments used by ClusterFuzz:
    parser.add_argument(
        '-n',
        '--no_of_files',
        type=int,
        required=True,
        help='The number of test cases that the fuzzer is '
        'expected to generate')
    parser.add_argument(
        '-i',
        '--input_dir',
        help='The directory containing the fuzzer\'s data '
        'bundle.')
    parser.add_argument(
        '-o',
        '--output_dir',
        required=True,
        help='The directory where test case files should be '
        'written to.')

    parser.add_argument(
        '--content_shell_dir',
        help='The directory of content shell. If present the '
        'program will print a command to run the '
        'generated test file.')

    return parser.parse_args()


def FuzzTemplate(template_path, resources_path):
    """Uses a template to return a test case that can be run as a web test.

    This functions reads the template in |template_path|, injects the necessary
    js files to run as a web test and fuzzes the template's parameters to
    generate a test case.

    Args:
      template_path: The path to the template that will be used to generate
          a new test case.
      resources_path: Path to the js files that need to be included.

    Returns:
      A binary string containing the test case.
    """
    print('Generating test file based on {}'.format(template_path))

    # Read the template.
    with open(template_path, 'r', encoding='utf-8') as template_in:
        template_file_data = template_in.read()

    # Generate a test file based on the template.
    generated_test = test_case_fuzzer.GenerateTestFile(template_file_data)
    # Fuzz parameters.
    fuzzed_file_data = parameter_fuzzer.FuzzParameters(generated_test)

    # Add includes
    for (js_file_name, include_parameter) in JS_FILES_AND_PARAMETERS:
        # Read js file.
        js_file_path = os.path.join(resources_path, js_file_name)
        with open(js_file_path, 'r', encoding='utf-8') as js_in:
            js_file_data = js_in.read()

        js_file_data = (SCRIPT_PREFIX + js_file_data + SCRIPT_SUFFIX)

        fuzzed_file_data = FillInParameter(
            include_parameter,
            lambda data=js_file_data: data,
            fuzzed_file_data)

    return fuzzed_file_data.encode('utf-8')


def WriteTestFile(test_file_data, test_file_prefix, output_dir):
    """Creates a new file with a unique name and writes the test case to it.

    Args:
      test_file_data: The data to be included in the new file.
      test_file_prefix: Used as a prefix when generating a new file.
      output_dir: The directory where the new file should be created.

    Returns:
      A string representing the file path to access the new file.
    """

    file_descriptor, file_path = tempfile.mkstemp(
        prefix=test_file_prefix, suffix='.html', dir=output_dir)

    with os.fdopen(file_descriptor, 'wb') as output:
        print('Writing {} bytes to \'{}\''.format(len(test_file_data),
                                                  file_path))
        output.write(test_file_data)

    return file_path


def main():
    args = _GetArguments()

    print('Generating {} test file(s).'.format(args.no_of_files))
    print('Writing test files to: \'{}\''.format(args.output_dir))
    if args.input_dir:
        print('Reading data bundle from: \'{}\''.format(args.input_dir))

    # Get Templates
    current_path = os.path.dirname(os.path.realpath(__file__))
    available_templates = glob.glob(
        os.path.join(current_path, 'templates', '*.html'))

    # Generate Test Files
    resources_path = os.path.join(current_path, 'resources')
    start_time = time.time()
    for file_no in range(args.no_of_files):
        template_path = available_templates[file_no % len(available_templates)]

        test_file_data = FuzzTemplate(template_path, resources_path)

        # Get Test File
        template_name = os.path.splitext(os.path.basename(template_path))[0]
        test_file_name = 'fuzz-{}-{}-{}'.format(template_name, int(start_time),
                                                int(file_no))

        test_file_path = WriteTestFile(test_file_data, test_file_name,
                                       args.output_dir)

        if args.content_shell_dir:
            print('{} --run-web-tests {}'.format(args.content_shell_dir,
                                                 test_file_path))


if __name__ == '__main__':
    sys.exit(main())
