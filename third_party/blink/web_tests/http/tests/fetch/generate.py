# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Generator script that, for each script-tests/X.js, creates
- window/X.html
- worker/X.html
- serviceworker/X.html
from templates in script-tests/TEMPLATE*.html.

The following tokens in the template files are replaced:
- TESTNAME -> X
- OPTIONS -> OPTIONS string (see README).

Run
$ python generate.py
at this (/LayoutTests/http/tests/fetch/) directory, and
commit the generated files.
'''

import os
import os.path
import re
import sys

top_path = os.path.dirname(os.path.abspath(__file__))
script_tests_path = os.path.join(top_path, 'script-tests')


def generate(output_path, template_path, context, testname, options):
    output_basename = testname + options + '.html'

    with open(template_path, 'rb') as template_file:
        template_data = template_file.read()
        output_data = re.sub(r'TESTNAME', testname, template_data)
        output_data = re.sub(r'OPTIONS', options, output_data)

    with open(os.path.join(output_path, output_basename), 'wb') as output_file:
        output_file.write(output_data)


def generate_directory(relative_path, contexts, original_options):
    directory_path = os.path.join(script_tests_path, relative_path)
    for script in os.listdir(directory_path):
        if script.startswith('.') or not script.endswith('.js'):
            continue
        testname = re.sub(r'\.js$', '', os.path.basename(script))
        options = original_options

        # Read OPTIONS list.
        with open(os.path.join(directory_path, script), 'rb') as script_file:
            script = script_file.read()
            options_match = re.search(r'// *OPTIONS: *([a-z\-,]*)', script)
            if options_match:
                options = re.split(',', options_match.group(1))

        for context in contexts:
            template_path = os.path.join(
                directory_path, 'TEMPLATE-' + context + '.html')
            for option in options:
                generate(os.path.join(top_path, context, relative_path), template_path, context, testname, option)


def main():
    basic_contexts = ['window', 'workers', 'serviceworker']

    generate_directory('', ['window', 'workers', 'serviceworker'],
                       ['', '-base-https-other-https'])
    generate_directory(
        'thorough',
        ['window', 'workers', 'serviceworker', 'serviceworker-proxied'],
        ['', '-other-https', '-base-https-other-https'])
    return 0

if __name__ == "__main__":
    sys.exit(main())
