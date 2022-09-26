#!/usr/bin/env vpython3
#
# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""Prints a list of test expectations for tests whose bugs haven't been modified recently."""

import csv
import datetime
import json
import optparse
from six import StringIO
import sys
from six.moves import urllib

from blinkpy.common.host import Host
# pylint: disable=no-name-in-module
from blinkpy.web_tests.models.test_expectations import TestExpectationParser

# FIXME: Make this a direct request to Monorail.
GOOGLE_CODE_URL = 'https://www.googleapis.com/projecthosting/v2/projects/chromium/issues/%s?key=AIzaSyDgCqT1Dt5AZWLHo4QJjyMHaCjhnFacGF0'
CRBUG_PREFIX = 'crbug.com/'
CSV_ROW_HEADERS = [
    'crbug link', 'test file', 'days since last update', 'owner', 'status'
]


class BugInfo():
    def __init__(self, bug_link, filename, days_since_last_update, owner,
                 status):
        self.bug_link = bug_link
        self.filename = filename
        self.days_since_last_update = days_since_last_update
        self.owner = owner
        self.status = status


class StaleTestPrinter(object):
    def __init__(self, options):
        self.days = options.days
        self.csv_filename = options.create_csv
        self.host = Host()
        self.bug_info = {}

    def print_stale_tests(self):
        port = self.host.port_factory.get()
        expectations = port.expectations_dict()
        parser = TestExpectationParser(port, all_tests=(), is_lint_mode=False)
        expectations_file, expectations_contents = expectations.items()[0]
        expectation_lines = parser.parse(expectations_file,
                                         expectations_contents)
        csv_rows = []
        for line in expectation_lines:
            row = self.check_expectations_line(line)
            if row:
                csv_rows.append(row)
        if self.csv_filename:
            self.write_csv(csv_rows)

    def write_csv(self, rows):
        out = StringIO.StringIO()
        writer = csv.writer(out)
        writer.writerow(CSV_ROW_HEADERS)
        for row in rows:
            writer.writerow(row)
        self.host.filesystem.write_text_file(self.csv_filename, out.getvalue())

    def check_expectations_line(self, line):
        """Checks the bugs in one test expectations line to see if they're stale.

        Args:
            line: A TestExpectationsLine instance.

        Returns:
            A CSV row (a list of strings), or None if there are no stale bugs.
        """
        bug_links, test_name = line.bugs, line.name
        try:
            if bug_links:
                # Prepopulate bug info.
                for bug_link in bug_links:
                    self.populate_bug_info(bug_link, test_name)
                # Return the stale bug's information.
                if all(self.is_stale(bug_link) for bug_link in bug_links):
                    print(line.original_string.strip())
                    return [
                        bug_links[0], self.bug_info[bug_links[0]].filename,
                        self.bug_info[bug_links[0]].days_since_last_update,
                        self.bug_info[bug_links[0]].owner,
                        self.bug_info[bug_links[0]].status
                    ]
        except urllib.error.HTTPError as error:
            if error.code == 404:
                message = 'got 404, bug does not exist.'
            elif error.code == 403:
                message = 'got 403, not accessible. Not able to tell if it\'s stale.'
            else:
                message = str(error)
            print(
                'Error when checking %s: %s' % (','.join(bug_links), message),
                sys.stderr)
        return None

    def populate_bug_info(self, bug_link, test_name):
        if bug_link in self.bug_info:
            return
        # In case there's an error in the request, don't make the same request again.
        bug_number = bug_link.strip(CRBUG_PREFIX)
        url = GOOGLE_CODE_URL % bug_number
        response = urllib.request.urlopen(url)
        parsed = json.loads(response.read())
        parsed_time = datetime.datetime.strptime(
            parsed['updated'].split(".")[0] + "UTC", "%Y-%m-%dT%H:%M:%S%Z")
        time_delta = datetime.datetime.now() - parsed_time
        owner = 'none'
        if 'owner' in parsed.keys():
            owner = parsed['owner']['name']
        self.bug_info[bug_link] = BugInfo(bug_link, test_name, time_delta.days,
                                          owner, parsed['state'])

    def is_stale(self, bug_link):
        return self.bug_info[bug_link].days_since_last_update > self.days


def main(argv):
    option_parser = optparse.OptionParser()
    option_parser.add_option(
        '--days',
        type='int',
        default=90,
        help='Number of days to consider a bug stale.')
    option_parser.add_option(
        '--create-csv',
        type='string',
        default='',
        help=
        'Filename of CSV file to write stale entries to. No file will be written if no name specified.'
    )
    options, _ = option_parser.parse_args(argv)
    printer = StaleTestPrinter(options)
    printer.print_stale_tests()
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
