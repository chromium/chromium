# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Start and stop the WPTserve servers as they're used by the web tests."""

import datetime
import json
import logging

import six

from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.servers import server_base

_log = logging.getLogger(__name__)


class WPTServe(server_base.ServerBase):
    def __init__(self, port_obj, output_dir):
        super(WPTServe, self).__init__(port_obj, output_dir)

        # These ports must match external/wpt/config.json
        http_port = 8001
        http_alt_port = 8081
        http_private_port = 8082
        http_public_port = 8093
        https_port = 8444
        https_alt_port = 8445
        https_private_port = 8446
        https_public_port = 8447
        h2_port = 9000
        ws_port = 9001
        wss_port = 9444
        webtransport_h3_port = 11000

        self._name = 'wptserve'
        self._log_prefixes = ('wptserve_stderr', )
        self._mappings = [{
            'port': http_port,
            'scheme': 'http'
        }, {
            'port': http_alt_port,
            'scheme': 'http'
        }, {
            'port': http_private_port,
            'scheme': 'http'
        }, {
            'port': http_public_port,
            'scheme': 'http'
        }, {
            'port': https_port,
            'scheme': 'https',
            'sslcert': True
        }, {
            'port': https_alt_port,
            'scheme': 'https',
            'sslcert': True
        }, {
            'port': https_private_port,
            'scheme': 'https'
        }, {
            'port': https_public_port,
            'scheme': 'https'
        }, {
            'port': h2_port,
            'scheme': 'https',
            'sslcert': True
        }, {
            'port': ws_port,
            'scheme': 'ws'
        }, {
            'port': wss_port,
            'scheme': 'wss',
            'sslcert': True
        }]

        # TODO(burnik): We can probably avoid PID files for WPT in the future.
        fs = self._filesystem
        self._pid_file = fs.join(self._runtime_path, '%s.pid' % self._name)
        self._config_file = fs.join(self._runtime_path, 'config.json')

        finder = PathFinder(fs)
        path_to_pywebsocket = finder.path_from_chromium_base(
            'third_party', 'pywebsocket3', 'src')
        self.path_to_wpt_support = finder.path_from_chromium_base(
            'third_party', 'wpt_tools')
        path_to_wpt_root = fs.join(self.path_to_wpt_support, 'wpt')
        wpt_script = fs.join(path_to_wpt_root, 'wpt')
        start_cmd = [
            self._port_obj.python3_command(),
            '-u',
            wpt_script,
            'serve',
            '--config',
            self._config_file,
            '--doc_root',
            finder.path_from_wpt_tests(),
        ]

        path_to_ws_handlers = finder.path_from_wpt_tests(
            'websockets', 'handlers')
        if self._port_obj.host.filesystem.exists(path_to_ws_handlers):
            start_cmd += ['--ws_doc_root', path_to_ws_handlers]

        if six.PY3:
            self._mappings.append({
                'port': webtransport_h3_port,
                'scheme': 'webtransport-h3'
            })
            start_cmd.append('--webtransport-h3')

        # TODO(burnik): We should stop setting the CWD once WPT can be run without it.
        self._cwd = finder.path_from_web_tests()
        self._env = port_obj.host.environ.copy()
        self._env.update({'PYTHONPATH': path_to_pywebsocket})
        self._start_cmd = start_cmd

        self._error_log_path = self._filesystem.join(output_dir,
                                                     'wptserve_stderr.txt')
        self._output_log_path = self._filesystem.join(output_dir,
                                                      'wptserve_stdout.txt')

        expiration_date = datetime.date(2025, 1, 4)
        if datetime.date.today() > expiration_date - datetime.timedelta(30):
            _log.error(
                'Pre-generated keys and certificates are going to be expired at %s.'
                ' Please re-generate them by following steps in %s/README.chromium.',
                expiration_date.strftime('%b %d %Y'), self.path_to_wpt_support)

    def _prepare_config(self):
        fs = self._filesystem
        finder = PathFinder(fs)
        template_path = finder.path_from_wpt_tests('config.json')
        config = json.loads(fs.read_text_file(template_path))
        for alias in config['aliases']:
            if alias['url-path'] == "/resources/testdriver-vendor.js":
                alias['local-dir'] = "resources"
        config['aliases'].append({
            'url-path':
            '/gen/',
            'local-dir':
            self._port_obj.generated_sources_directory()
        })

        with fs.open_text_file_for_writing(self._config_file) as f:
            json.dump(config, f)

        # wptserve is spammy on stderr even at the INFO log level and will block
        # the pipe, so we need to redirect it.
        # The file is opened here instead in __init__ because _remove_stale_logs
        # will try to delete the log file, which causes deadlocks on Windows.
        self._stderr = fs.open_text_file_for_writing(self._error_log_path)

        # The pywebsocket process started by wptserve logs to stdout. This can
        # also cause deadlock, and so should also be redirected to a file.
        self._stdout = fs.open_text_file_for_writing(self._output_log_path)

    def _stop_running_server(self):
        if not self._wait_for_action(self._check_and_kill):
            # This is mostly for POSIX systems. We send SIGINT in
            # _check_and_kill() and here we use SIGKILL.
            self._executive.kill_process(self._pid)

        if self._filesystem.exists(self._pid_file):
            self._filesystem.remove(self._pid_file)
        if self._filesystem.exists(self._config_file):
            self._filesystem.remove(self._config_file)

    def _check_and_kill(self):
        """Tries to kill wptserve.

        Returns True if it appears to be not running. Or, if it appears to be
        running, tries to kill the process and returns False.
        """
        if not self._pid:
            _log.warning('No PID; wptserve has not started.')
            return True

        # Polls the process in case it has died; otherwise, the process might be
        # defunct and check_running_pid can still succeed.
        if (self._process and self._process.poll()) or \
                (not self._executive.check_running_pid(self._pid)):
            _log.debug('pid %d is not running', self._pid)
            return True

        _log.debug('pid %d is running, killing it', self._pid)
        self._executive.kill_process(self._pid)

        return False
