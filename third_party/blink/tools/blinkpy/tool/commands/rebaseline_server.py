# Copyright (c) 2010 Google Inc. All rights reserved.
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
"""Starts a local HTTP server which displays web test failures (given a test
results directory), provides comparisons of expected and actual results (both
images and text) and allows one-click rebaselining of tests.
"""

from blinkpy.common.host import Host
from blinkpy.common.net.web_test_results import WebTestResults
from blinkpy.web_tests.layout_package import json_results_generator
from blinkpy.tool.commands.abstract_local_server_command import AbstractLocalServerCommand
from blinkpy.tool.servers.rebaseline_server import get_test_baselines, RebaselineHTTPServer, STATE_NEEDS_REBASELINE


class TestConfig(object):
    def __init__(self, test_port, web_tests_directory, results_directory,
                 platforms, host):
        self.test_port = test_port
        self.web_tests_directory = web_tests_directory
        self.results_directory = results_directory
        self.platforms = platforms
        self.host = host
        self.filesystem = host.filesystem
        self.git = host.git()


class RebaselineServer(AbstractLocalServerCommand):
    name = 'rebaseline-server'
    help_text = __doc__
    show_in_main_help = True
    argument_names = '/path/to/results/directory'

    server = RebaselineHTTPServer

    def __init__(self):
        super(RebaselineServer, self).__init__()
        self._test_config = None

    def _gather_baselines(self, results_json):
        # Rebaseline server and it's associated JavaScript expected the tests subtree to
        # be key-value pairs instead of hierarchical.
        # FIXME: make the rebaseline server use the hierarchical tree.
        new_tests_subtree = {}

        def gather_baselines_for_test(result):
            if result.did_pass_or_run_as_expected():
                return
            result_dict = result.result_dict()
            result_dict['state'] = STATE_NEEDS_REBASELINE
            result_dict['baselines'] = get_test_baselines(
                result.test_name(), self._test_config)
            new_tests_subtree[result.test_name()] = result_dict

        WebTestResults(results_json).for_each_test(gather_baselines_for_test)
        results_json['tests'] = new_tests_subtree

    def _prepare_config(self, options, args, tool):
        results_directory = args[0]
        host = Host()

        print 'Parsing full_results.json...'
        results_json_path = host.filesystem.join(results_directory,
                                                 'full_results.json')
        results_json = json_results_generator.load_json(
            host.filesystem, results_json_path)

        port = tool.port_factory.get()
        web_tests_directory = port.web_tests_dir()
        platforms = host.filesystem.listdir(
            host.filesystem.join(web_tests_directory, 'platform'))
        self._test_config = TestConfig(port, web_tests_directory,
                                       results_directory, platforms, host)

        print 'Gathering current baselines...'
        self._gather_baselines(results_json)

        return {
            'test_config': self._test_config,
            'results_json': results_json,
            'platforms_json': {
                'platforms': platforms,
                'defaultPlatform': port.name(),
            },
        }
