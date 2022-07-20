# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import multiprocessing
import signal
import traceback
from collections import namedtuple

from blinkpy.common.system.log_utils import configure_logging
from blinkpy.common.host import Host

_log = logging.getLogger('web_tests_history.py')


class HistoryChecker(object):
    # Full hash, committer UNIX timestamp, author email
    FORMAT = "%H %ct %ae"
    CommitInfo = namedtuple('CommitInfo', ['hash', 'time', 'author'])

    # d77503fce4933a5b1cf55f375b77d0a20afdf9e3
    BLINK_FORK_TIME = 1365030973

    def __init__(self, host, argv):
        self.host = host
        self.port = host.port_factory.get()
        self.filesystem = host.filesystem
        self.executive = host.executive

        self.options = self.parse_args(argv[1:])
        self._path = argv[0]

        if self.options.verbose >= 2:
            log_level = logging.DEBUG
        elif self.options.verbose == 1:
            log_level = logging.INFO
        else:
            log_level = logging.WARNING
        configure_logging(logging_level=log_level, include_time=False)

    @staticmethod
    def parse_args(args):
        parser = argparse.ArgumentParser(
            prog='web_tests_history.py',
            description='''Examine the version history of web tests to check
            if they were created before the Blink fork; and if so, whether all
            contributions were made by Googlers.''')
        parser.add_argument(
            '--verbose',
            '-v',
            action='count',
            default=0,
            help='show verbose logging (can be repeated, e.g. -vv)')
        parser.add_argument(
            'paths',
            metavar='PATH',
            type=str,
            nargs='*',
            help=
            'test path relative to web_tests (same as the arguments for run_web_tests.py); '
            'if no path is provided, the script will check all files.')
        return parser.parse_args(args)

    def git(self, args):
        command = ['git'] + args
        return self.executive.run_command(command)

    def run_git_log(self, path):
        # Follow rename and copy.
        output = self.git(
            ['log', '--follow', '-M', '-C', '--format=' + self.FORMAT, path])
        commits = []
        for line in output.splitlines():
            parts = line.split()
            assert len(parts) == 3
            # Make the timestamp an int so that we can compare them easily.
            commits.append(self.CommitInfo(parts[0], int(parts[1]), parts[2]))
        return commits

    def analyze(self, path, commits):
        before_fork = 0
        before_fork_googlers = 0
        for commit in commits:
            if commit.time < self.BLINK_FORK_TIME:
                before_fork += 1
                if ('google.com' in commit.author) or \
                        ('chromium.org' in commit.author):
                    before_fork_googlers += 1
        if before_fork == 0:
            return "%s\t[OK] created after fork" % path
        elif before_fork == before_fork_googlers:
            return "%s\t[OK] created before fork, but all pre-fork commits from Googlers" % path
        else:
            return "%s\t[NO]" % path

    def _process_single(self, path):
        _init(self)
        print(_run(path))
        return 0

    def _process_many(self, paths):
        files = self.port.real_tests(paths)
        if len(files) == 0:
            _log.error("No tests found.")
            return 1
        _log.info("Total test files discovered: %d", len(files))

        pool = multiprocessing.Pool(processes=self.executive.cpu_count(),
                                    initializer=_init,
                                    initargs=(self, ))

        # Capture SIGTERM/INT to exit gracefully without leaving workers behind.
        def _handler(signum, _):
            _log.error('Received signal %d, exiting...', signum)
            raise SystemExit

        signal.signal(signal.SIGINT, _handler)
        signal.signal(signal.SIGTERM, _handler)

        try:
            for res in pool.imap_unordered(_run, files):
                # BaseException includes Exception as well as KeyboardInterrupt.
                if isinstance(res, BaseException):
                    # Traceback is already printed in the worker; exit directly.
                    raise SystemExit
                print(res)
            pool.close()
        except Exception:
            # A user exception was raised from the manager (main) process.
            traceback.print_exc()
            pool.terminate()
            return 1
        except SystemExit:
            # Either a worker process has exited unexpectedly, or the manager
            # process has received SIGTERM/INT.
            pool.terminate()
            return 1
        finally:
            pool.join()
        return 0

    def _is_test(self, path):
        return self.port.is_non_wpt_test_file(
            self.filesystem.dirname(path), self.filesystem.basename(path))

    def process(self):
        if len(self.options.paths) == 1 and self._is_test(
                self.options.paths[0]):
            return self._process_single(self.options.paths[0])
        return self._process_many(self.options.paths)


# The following protected variable and functions are used inside worker
# processes. Functions have to be Picklable to work with multiprocessing.Pool,
# so they are defined at the module level, and thus the variable is global, too.
_checker = None


def _init(checker):
    global _checker
    _checker = checker


def _run(path):
    try:
        abs_path = _checker.filesystem.join(_checker.port.web_tests_dir(),
                                            path)
        commits = _checker.run_git_log(abs_path)
        return _checker.analyze(path, commits)
    except Exception as e:
        traceback.print_exc()
        return e
    except KeyboardInterrupt as e:
        return e


def main(argv):
    host = Host()
    checker = HistoryChecker(host, argv)
    return checker.process()
