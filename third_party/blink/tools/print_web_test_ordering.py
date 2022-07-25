#!/usr/bin/env vpython3

# Copyright (C) 2012 Google Inc. All rights reserved.
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

import json
import optparse
import sys

from blinkpy.common.host import Host


def main(argv):
    parser = optparse.OptionParser(usage='%prog [stats.json]')
    parser.description = "Prints out lists of tests run on each worker as per the stats.json file."
    options, args = parser.parse_args(argv)

    if args and args[0]:
        stats_path = args[0]
    else:
        host = Host()
        stats_path = host.filesystem.join(
            host.port_factory.get().artifacts_directory(), 'stats.json')

    with open(stats_path, 'r') as fp:
        stats_trie = json.load(fp)

    stats = convert_trie_to_flat_paths(stats_trie)
    stats_by_worker = {}
    for test_name, data in stats.items():
        worker = "worker/" + str(data["results"][0])
        if worker not in stats_by_worker:
            stats_by_worker[worker] = []
        test_number = data["results"][1]
        stats_by_worker[worker].append({
            "name": test_name,
            "number": test_number
        })

    for worker in sorted(stats_by_worker.keys()):
        print(worker + ':')
        for test in sorted(
                stats_by_worker[worker], key=lambda test: test["number"]):
            print(test["name"])
        print


def convert_trie_to_flat_paths(trie, prefix=None):
    # Cloned from blinkpy.web_tests.layout_package.json_results_generator
    # so that this code can stand alone.
    result = {}
    for name, data in trie.items():
        if prefix:
            name = prefix + "/" + name
        if "results" in data:
            result[name] = data
        else:
            result.update(convert_trie_to_flat_paths(data, name))

    return result


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
