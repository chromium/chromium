# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Start and stop the WPTserve servers as they're used by the web tests."""

import datetime
import json
import logging

from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.servers import server_base


_log = logging.getLogger(__name__)


class WPTServe(server_base.ServerBase):

    def __init__(self, port_obj, output_dir):
        super(WPTServe, self).__init__(port_obj, output_dir)
        # These ports must match wpt_support/wpt.config.json
        http_port, http_alt_port, https_port = (8001, 8081, 8444)
        ws_port, wss_port = (9001, 9444)
        self._name = 'wptserve'
        self._log_prefixes = ('wptserve_stderr', )
        self._mappings = [{'port': http_port, 'scheme': 'http'},
                          {'port': http_alt_port, 'scheme': 'http'},
                          {'port': https_port, 'scheme': 'https', 'sslcert': True},
                          {'port': ws_port, 'scheme': 'ws'},
                          {'port': wss_port, 'scheme': 'wss', 'sslcert': True}]

        # TODO(burnik): We can probably avoid PID files for WPT in the future.
        fs = self._filesystem
        self._pid_file = fs.join(self._runtime_path, '%s.pid' % self._name)

        finder = PathFinder(fs)
        path_to_pywebsocket = finder.path_from_chromium_base('third_party', 'pywebsocket', 'src')
        path_to_wpt_support = finder.path_from_blink_tools('blinkpy', 'third_party', 'wpt')
        path_to_wpt_root = fs.join(path_to_wpt_support, 'wpt')
        path_to_wpt_tests = fs.abspath(fs.join(self._port_obj.web_tests_dir(), 'external', 'wpt'))
        path_to_ws_handlers = fs.join(path_to_wpt_tests, 'websockets', 'handlers')
        self._config_file = self._prepare_wptserve_config(path_to_wpt_support)
        wpt_script = fs.join(path_to_wpt_root, 'wpt')
        start_cmd = [self._port_obj.host.executable,
                     '-u', wpt_script, 'serve',
                     '--config', self._config_file,
                     '--doc_root', path_to_wpt_tests]

        # Some users (e.g. run_webdriver_tests.py) do not need WebSocket
        # handlers, so we only add the flag if the directory exists.
        if self._port_obj.host.filesystem.exists(path_to_ws_handlers):
            start_cmd += ['--ws_doc_root', path_to_ws_handlers]

        # TODO(burnik): We should stop setting the CWD once WPT can be run without it.
        self._cwd = path_to_wpt_root
        self._env = port_obj.host.environ.copy()
        self._env.update({'PYTHONPATH': path_to_pywebsocket})
        self._start_cmd = start_cmd

        self._error_log_path = self._filesystem.join(output_dir, 'wptserve_stderr.txt')

        expiration_date = datetime.date(2025, 1, 4)
        if datetime.date.today() > expiration_date - datetime.timedelta(30):
            _log.error(
                'Pre-generated keys and certificates are going to be expired at %s.'
                ' Please re-generate them by following steps in %s/README.chromium.',
                expiration_date.strftime('%b %d %Y'), path_to_wpt_support)

    def _prepare_wptserve_config(self, path_to_wpt_support):
        fs = self._filesystem
        template_path = fs.join(path_to_wpt_support, 'wpt.config.json')
        config = json.loads(fs.read_text_file(template_path))
        config['aliases'].append({
            'url-path': '/gen/',
            'local-dir': self._port_obj.generated_sources_directory()
        })

        f, temp_file = fs.open_text_tempfile('.json')
        json.dump(config, f)
        f.close()
        return temp_file

    def _prepare_config(self):
        # wptserve is spammy on stderr even at the INFO log level and will block
        # the pipe, so we need to redirect it.
        # The file is opened here instead in __init__ because _remove_stale_logs
        # will try to delete the log file, which causes deadlocks on Windows.
        self._stderr = self._filesystem.open_text_file_for_writing(self._error_log_path)

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
        if not (self._pid and self._process):
            _log.warning('No process object or PID. wptserve has not started.')
            return True

        # Polls the process in case it has died; otherwise, the process might be
        # defunct and check_running_pid can still succeed.
        if self._process.poll() or not self._executive.check_running_pid(self._pid):
            _log.debug('pid %d is not running', self._pid)
            return True

        _log.debug('pid %d is running, killing it', self._pid)
        self._executive.kill_process(self._pid)

        return False
