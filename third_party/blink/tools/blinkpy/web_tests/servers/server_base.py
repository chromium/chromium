# Copyright (C) 2011 Google Inc. All rights reserved.
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
"""Base class used to start servers used by the web tests."""

import errno
import logging
import socket
import sys
import tempfile

import six

_log = logging.getLogger(__name__)

# This module is dynamically imported when the WebTransport over HTTP/3 server
# is enabled. It only works with python3.
# pylint: disable=invalid-name
webtransport_h3_server = None
# pylint: enable=invalid-name


def _is_webtransport_h3_server_running(port):
    if six.PY2:
        # TODO(crbug.com/1250210): Consider to support python2.
        return False

    # Import the WebTransport server module from wpt tools.
    global webtransport_h3_server
    if webtransport_h3_server is None:
        from blinkpy.common.path_finder import get_wpt_tools_wpt_dir
        wpt_tools_path = get_wpt_tools_wpt_dir()
        if wpt_tools_path not in sys.path:
            sys.path.insert(0, wpt_tools_path)

        from importlib import import_module
        webtransport_h3_server = import_module(
            'tools.webtransport.h3.webtransport_h3_server')

    return webtransport_h3_server.server_is_running(host='127.0.0.1',
                                                    port=port,
                                                    timeout=1)


class ServerError(Exception):
    pass


class ServerBase(object):
    """A skeleton class for starting and stopping servers used by the web tests."""

    def __init__(self, port_obj, output_dir):
        self._port_obj = port_obj
        self._executive = port_obj.host.executive
        self._filesystem = port_obj.host.filesystem
        self._platform = port_obj.host.platform
        self._output_dir = output_dir

        # On Mac and Linux tmpdir is set to '/tmp' for (i) consistency and
        # (ii) because it is hardcoded in the Apache ScoreBoardFile directive.
        tmpdir = tempfile.gettempdir()
        if self._platform.is_mac() or self._platform.is_linux():
            tmpdir = '/tmp'

        self._runtime_path = self._filesystem.join(tmpdir, 'WebKit')
        self._filesystem.maybe_make_directory(self._runtime_path)
        self._filesystem.maybe_make_directory(self._output_dir)

        # Subclasses must override these fields.
        self._name = '<virtual>'
        self._log_prefixes = tuple()
        self._mappings = {}
        self._pid_file = None
        self._start_cmd = None

        # Subclasses may override these fields.
        self._env = None
        self._cwd = None
        # TODO(robertma): There is a risk of deadlocks since we don't read from
        # the pipes until the subprocess exits. For now, subclasses need to
        # either make sure server processes don't spam on stdout/stderr, or
        # redirect them to files.
        self._stdout = self._executive.PIPE
        self._stderr = self._executive.PIPE
        # The entrypoint process of the server, which may not be the daemon,
        # e.g. apachectl.
        self._process = None
        # The PID of the server daemon, which may be different from
        # self._process.pid.
        self._pid = None
        self._error_log_path = None

    def start(self):
        """Starts the server. It is an error to start an already started server.

        This method also stops any stale servers started by a previous instance.
        """
        assert not self._pid, '%s server is already running' % self._name

        # Stop any stale servers left over from previous instances.
        if self._filesystem.exists(self._pid_file):
            try:
                self._pid = int(
                    self._filesystem.read_text_file(self._pid_file))
                _log.debug('stale %s pid file, pid %d', self._name, self._pid)
                self._stop_running_server()
            except (ValueError, UnicodeDecodeError):
                # These could be raised if the pid file is corrupt.
                self._remove_pid_file()
            self._pid = None

        self._remove_stale_logs()
        self._prepare_config()
        self._check_that_all_ports_are_available()

        self._pid = self._spawn_process()

        if self._wait_for_action(self._is_server_running_on_all_ports):
            _log.debug('%s successfully started (pid = %d)', self._name,
                       self._pid)
        else:
            self._log_errors_from_subprocess()
            self._stop_running_server()
            raise ServerError('Failed to start %s server' % self._name)

    def stop(self):
        """Stops the server. Stopping a server that isn't started is harmless."""
        actual_pid = None
        try:
            if self._filesystem.exists(self._pid_file):
                try:
                    actual_pid = int(
                        self._filesystem.read_text_file(self._pid_file))
                except (ValueError, UnicodeDecodeError):
                    # These could be raised if the pid file is corrupt.
                    pass
                if not self._pid:
                    self._pid = actual_pid

            if not self._pid:
                return

            if not actual_pid:
                _log.warning('Failed to stop %s: pid file is missing',
                             self._name)
                return
            if self._pid != actual_pid:
                _log.warning('Failed to stop %s: pid file contains %d, not %d',
                             self._name, actual_pid, self._pid)
                # Try to kill the existing pid, anyway, in case it got orphaned.
                self._executive.kill_process(self._pid)
                self._pid = None
                return

            _log.debug('Attempting to shut down %s server at pid %d',
                       self._name, self._pid)
            self._stop_running_server()
            _log.debug('%s server at pid %d stopped', self._name, self._pid)
            self._pid = None
        finally:
            # Make sure we delete the pid file no matter what happens.
            self._remove_pid_file()

    def alive(self):
        """Checks whether the server is alive."""
        # This by default checks both the process and ports.
        # At this point, we think the server has started up, so successes are
        # normal while failures are not.
        return self._is_server_running_on_all_ports(
            success_log_level=logging.DEBUG, failure_log_level=logging.INFO)

    def _prepare_config(self):
        """This routine can be overridden by subclasses to do any sort
        of initialization required prior to starting the server that may fail.
        """

    def _remove_stale_logs(self):
        """This routine can be overridden by subclasses to try and remove logs
        left over from a prior run. This routine should log warnings if the
        files cannot be deleted, but should not fail unless failure to
        delete the logs will actually cause start() to fail.
        """
        # Sometimes logs are open in other processes but they should clear eventually.
        for log_prefix in self._log_prefixes:
            try:
                self._remove_log_files(self._output_dir, log_prefix)
            except OSError:
                _log.exception('Failed to remove old %s %s files', self._name,
                               log_prefix)

    def _spawn_process(self):
        _log.debug('Starting %s server, cmd="%s"', self._name, self._start_cmd)
        self._process = self._executive.popen(
            self._start_cmd,
            env=self._env,
            cwd=self._cwd,
            stdout=self._stdout,
            stderr=self._stderr)
        pid = self._process.pid
        self._filesystem.write_text_file(self._pid_file, str(pid))
        return pid

    def _stop_running_server(self):
        self._wait_for_action(self._check_and_kill)
        if self._filesystem.exists(self._pid_file):
            self._filesystem.remove(self._pid_file)

    def _check_and_kill(self):
        if self._executive.check_running_pid(self._pid):
            _log.debug('pid %d is running, killing it', self._pid)
            self._executive.kill_process(self._pid)
            return False
        else:
            _log.debug('pid %d is not running', self._pid)

        return True

    def _remove_pid_file(self):
        if self._filesystem.exists(self._pid_file):
            self._filesystem.remove(self._pid_file)

    def _remove_log_files(self, folder, starts_with):
        files = self._filesystem.listdir(folder)
        for file in files:
            if file.startswith(starts_with):
                full_path = self._filesystem.join(folder, file)
                self._filesystem.remove(full_path)

    def _log_errors_from_subprocess(self):
        _log.error('logging %s errors, if any', self._name)
        if self._process:
            _log.error('%s returncode %s', self._name,
                       str(self._process.returncode))
            if self._process.stderr:
                stderr_text = self._process.stderr.read()
                if stderr_text:
                    _log.error('%s stderr:', self._name)
                    for line in stderr_text.splitlines():
                        _log.error('  %s', line)
                else:
                    _log.error('%s no stderr', self._name)
            else:
                _log.error('%s no stderr handle', self._name)
        else:
            _log.error('%s no process', self._name)
        if self._error_log_path and self._filesystem.exists(
                self._error_log_path):
            error_log_text = self._filesystem.read_text_file(
                self._error_log_path)
            if error_log_text:
                _log.error('%s error log (%s) contents:', self._name,
                           self._error_log_path)
                for line in error_log_text.splitlines():
                    _log.error('  %s', line)
            else:
                _log.error('%s error log empty', self._name)
            _log.error('')
        else:
            _log.error('%s no error log', self._name)

    def _wait_for_action(self, action, wait_secs=20.0, sleep_secs=1.0):
        """Repeat the action for wait_sec or until it succeeds, sleeping for sleep_secs
        in between each attempt. Returns whether it succeeded.
        """
        start_time = self._port_obj.host.time()
        while self._port_obj.host.time() - start_time < wait_secs:
            if action():
                return True
            _log.debug('Waiting for action: %s', action)
            self._port_obj.host.sleep(sleep_secs)

        return False

    def _is_server_running_on_all_ports(self,
                                        success_log_level=logging.INFO,
                                        failure_log_level=logging.DEBUG):
        """Returns whether the server is running on all the desired ports.

        Args:
            success_log_level: Logging level for success (default: INFO)
            failure_log_level: Logging level for failure (default: DEBUG)
        """
        # Check self._pid instead of self._process because the latter might be a
        # control process that exits after spawning up the daemon.
        # TODO(dpranke): crbug/378444 maybe pid is unreliable on win?
        if (not self._platform.is_win()
                and not self._executive.check_running_pid(self._pid)):
            _log.debug("Server isn't running at all")
            self._log_errors_from_subprocess()
            raise ServerError('Server exited')

        for mapping in self._mappings:
            port = mapping['port']
            scheme = mapping['scheme']
            if scheme == 'webtransport-h3':
                if not _is_webtransport_h3_server_running(port):
                    _log.log(failure_log_level,
                             'WebTransportH3 server NOT running on '\
                             'https://localhost:%d', port)
                    return False
                _log.log(
                    success_log_level,
                    'WebTransportH3 server running on https://localhost:%d',
                    port)
                continue
            s = socket.socket()
            try:
                s.connect(('localhost', port))
                _log.log(success_log_level,
                         'Server running on %s://localhost:%d', scheme, port)
            except IOError as error:
                if error.errno not in (errno.ECONNREFUSED, errno.ECONNRESET):
                    raise
                _log.log(failure_log_level,
                         'Server NOT running on %s://localhost:%d : %s',
                         scheme, port, error)
                return False
            finally:
                s.close()
        return True

    def _check_that_all_ports_are_available(self):
        for mapping in self._mappings:
            scheme = mapping['scheme']
            if scheme == 'webtransport-h3':
                socket_type = socket.SOCK_DGRAM
            else:
                socket_type = socket.SOCK_STREAM
            s = socket.socket(socket.AF_INET, socket_type)
            if not self._platform.is_win():
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            port = mapping['port']
            try:
                s.bind(('localhost', port))
            except IOError as error:
                if error.errno in (errno.EALREADY, errno.EADDRINUSE):
                    raise ServerError('Port %d is already in use.' % port)
                elif self._platform.is_win() and error.errno in (
                        errno.WSAEACCES, ):  # pylint: disable=no-member
                    raise ServerError('Port %d is already in use.' % port)
                else:
                    raise
            finally:
                s.close()
        _log.debug('all ports are available')
