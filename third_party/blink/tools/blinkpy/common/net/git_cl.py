# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An interface to git-cl.

The git-cl tool is responsible for communicating with Rietveld, Gerrit,
and Buildbucket to manage changelists and try jobs associated with them.
"""

import collections
import json
import logging
import re

from blinkpy.common.net.buildbot import Build, filter_latest_builds
from blinkpy.common.checkout.git import Git

_log = logging.getLogger(__name__)

# A refresh token may be needed for some commands, such as git cl try,
# in order to authenticate with buildbucket.
_COMMANDS_THAT_TAKE_REFRESH_TOKEN = ('try',)


class CLStatus(collections.namedtuple('CLStatus', ('status', 'try_job_results'))):
    """Represents the current status of a particular CL.

    It contains both the CL's status as reported by `git-cl status' as well as
    a mapping of Build objects to TryJobStatus objects.
    """
    pass


class TryJobStatus(collections.namedtuple('TryJobStatus', ('status', 'result'))):
    """Represents a current status of a particular job.

    Specifically, whether it is scheduled or started or finished, and if
    it is finished, whether it failed or succeeded. If it failed,
    """
    def __new__(cls, status, result=None):
        assert status in ('SCHEDULED', 'STARTED', 'COMPLETED')
        assert result in (None, 'FAILURE', 'SUCCESS', 'CANCELED')
        return super(TryJobStatus, cls).__new__(cls, status, result)


class GitCL(object):

    def __init__(self, host, auth_refresh_token_json=None, cwd=None):
        self._host = host
        self._auth_refresh_token_json = auth_refresh_token_json
        self._cwd = cwd
        self._git_executable_name = Git.find_executable_name(host.executive, host.platform)

    def run(self, args):
        """Runs git-cl with the given arguments and returns the output."""
        command = [self._git_executable_name, 'cl'] + args
        if self._auth_refresh_token_json and args[0] in _COMMANDS_THAT_TAKE_REFRESH_TOKEN:
            command += ['--auth-refresh-token-json', self._auth_refresh_token_json]
        return self._host.executive.run_command(command, cwd=self._cwd)

    def trigger_try_jobs(self, builders, bucket=None):
        """Triggers try jobs on the given builders.

        Args:
            builder: A list of builder names.
            bucket: When specified, all jobs are triggered to be in this bucket
                (instead of the configured or default buckets).
        """
        if bucket:
            builders_by_bucket = {bucket: builders}
        else:
            builders_by_bucket = self._group_builders_by_bucket(builders)
        # Sort both buckets and builders to ensure stable unit tests.
        for bucket in sorted(builders_by_bucket):
            command = ['try']
            # Only specify bucket if it's explicitly given to us. Otherwise,
            # `git cl` will figure out the appropriate bucket.
            if bucket:
                command.extend(['-B', bucket])
            for builder in sorted(builders_by_bucket[bucket]):
                command.extend(['-b', builder])
            self.run(command)

    def _group_builders_by_bucket(self, builders):
        builders_by_bucket = collections.defaultdict(list)
        for builder in builders:
            bucket = self._host.builders.bucket_for_builder(builder)
            builders_by_bucket[bucket].append(builder)
        return dict(builders_by_bucket)

    def get_issue_number(self):
        """Returns the issue number as a string, or "None"."""
        # Expected output of git cl issue looks like:
        # "<Optional message> Issue number: 1234 (<url>)".
        # Note: git cl gets the number from local git config, e.g.
        #   by running `git config branch.<branchname>.gerritissue`.
        output = self.run(['issue']).split()
        if 'number:' in output:
            return output[output.index('number:') + 1]
        return 'None'

    def _get_cl_status(self):
        return self.run(['status', '--field=status']).strip()

    def wait_for_try_jobs(
            self, poll_delay_seconds=10 * 60, timeout_seconds=120 * 60,
            cq_only=False):
        """Waits until all try jobs are finished and returns results, or None.

        This function can also be interrupted if the corresponding CL is
        closed while the try jobs are still running.

        Returns:
            None if a timeout occurs, a CLStatus tuple otherwise.
        """

        def finished_try_job_results_or_none():
            cl_status = self._get_cl_status()
            _log.debug('Fetched CL status: %s', cl_status)
            try_job_results = self.latest_try_jobs(cq_only=cq_only)
            _log.debug('Fetched try results: %s', try_job_results)
            if (cl_status == 'closed' or
                    (try_job_results and self.all_finished(try_job_results))):
                return CLStatus(status=cl_status,
                                try_job_results=try_job_results)
            return None

        return self._wait_for(
            finished_try_job_results_or_none,
            poll_delay_seconds, timeout_seconds,
            message=' for try jobs')

    def wait_for_closed_status(self, poll_delay_seconds=2 * 60, timeout_seconds=30 * 60):
        """Waits until git cl reports that the current CL is closed."""

        def closed_status_or_none():
            status = self._get_cl_status()
            if status == 'closed':
                self._host.print_('CL is closed.')
                return status
            return None

        return self._wait_for(
            closed_status_or_none,
            poll_delay_seconds, timeout_seconds,
            message=' for closed status')

    def _wait_for(self, poll_function, poll_delay_seconds, timeout_seconds, message=''):
        """Waits for the given poll_function to return something other than None.

        Args:
            poll_function: A function with no args that returns something
                when ready, or None when not ready.
            poll_delay_seconds: Time to wait between fetching results.
            timeout_seconds: Time to wait before aborting.
            message: Message to print indicate what is being waited for.

        Returns:
            The value returned by poll_function, or None on timeout.
        """
        start = self._host.time()
        self._host.print_(
            'Waiting%s, timeout: %d seconds.' % (message, timeout_seconds))
        while (self._host.time() - start) < timeout_seconds:
            self._host.sleep(poll_delay_seconds)
            value = poll_function()
            if value is not None:
                return value
            self._host.print_(
                'Waiting%s. %d seconds passed.' %
                (message, self._host.time() - start))
            self._host.sleep(poll_delay_seconds)
        self._host.print_('Timed out waiting%s.' % message)
        return None

    def latest_try_jobs(self, builder_names=None, cq_only=False, patchset=None):
        """Fetches a dict of Build to TryJobStatus for the latest try jobs.

        This includes jobs that are not yet finished and builds with infra
        failures, so if a build is in this list, that doesn't guarantee that
        there are results.

        Args:
            builder_names: Optional list of builders used to filter results.
            cq_only: If True, only include CQ jobs.
            patchset: If given, use this patchset instead of the latest.

        Returns:
            A dict mapping Build objects to TryJobStatus objects, with
            only the latest jobs included.
        """
        # TODO(crbug.com/771438): Update filter_latest to handle Swarming tasks.
        return self.filter_latest(
            self.try_job_results(
                builder_names, include_swarming_tasks=False, cq_only=cq_only,
                patchset=patchset))

    @staticmethod
    def filter_latest(try_results):
        """Returns the latest entries from from a Build to TryJobStatus dict."""
        if try_results is None:
            return None
        latest_builds = filter_latest_builds(try_results.keys())
        return {b: s for b, s in try_results.items() if b in latest_builds}

    def try_job_results(
            self, builder_names=None, include_swarming_tasks=True,
            cq_only=False, patchset=None):
        """Returns a dict mapping Build objects to TryJobStatus objects."""
        raw_results = self.fetch_raw_try_job_results(patchset=patchset)
        build_to_status = {}
        for result in raw_results:
            if builder_names and result['builder_name'] not in builder_names:
                continue
            is_swarming_task = result['url'] and '/task/' in result['url']
            if is_swarming_task and not include_swarming_tasks:
                continue
            is_cq = 'user_agent:cq' in result.get('tags', [])
            is_experimental = 'cq_experimental:true' in result.get('tags', [])
            if cq_only and not (is_cq and not is_experimental):
                continue
            build_to_status[self._build(result)] = self._try_job_status(result)
        return build_to_status

    def fetch_raw_try_job_results(self, patchset=None):
        """Requests results of try jobs for the current CL and the parsed JSON.

        The return value is expected to be a list of dicts, which each are
        expected to have the fields "builder_name", "status", "result", and
        "url". The format is determined by the output of "git cl try-results".
        """
        with self._host.filesystem.mkdtemp() as temp_directory:
            results_path = self._host.filesystem.join(temp_directory, 'try-results.json')
            command = ['try-results', '--json', results_path]
            if patchset:
                command.extend(['--patchset', str(patchset)])
            self.run(command)
            contents = self._host.filesystem.read_text_file(results_path)
            _log.debug('Fetched try results to file "%s".', results_path)
            self._host.filesystem.remove(results_path)
        return json.loads(contents)

    @staticmethod
    def _build(result_dict):
        """Converts a parsed try result dict to a Build object."""
        builder_name = result_dict['builder_name']
        url = result_dict['url']
        if url is None:
            return Build(builder_name, None)

        # LUCI jobs
        # TODO(martiniss): Switch to using build number once `git cl
        # try-results` uses buildbucket v2 API.
        tags = result_dict.get('tags', [])
        for tag in tags:
            if tag.startswith("build_address:"):
                build_number = tag.split('/')[-1]
                return Build(builder_name, int(build_number))

        # BuildBot jobs
        match = re.match(r'.*/builds/(\d+)/?$', url)
        if match:
            build_number = match.group(1)
            return Build(builder_name, int(build_number))

        # Swarming tasks
        match = re.match(r'.*/task/([0-9a-f]+)(/?|\?.*)$', url)
        assert match, '%s did not match expected format' % url
        task_id = match.group(1)
        return Build(builder_name, task_id)

    @staticmethod
    def _try_job_status(result_dict):
        """Converts a parsed try result dict to a TryJobStatus object."""
        return TryJobStatus(result_dict['status'], result_dict['result'])

    @staticmethod
    def all_finished(try_results):
        return all(s.status == 'COMPLETED' for s in try_results.values())

    @staticmethod
    def all_success(try_results):
        return all(s.status == 'COMPLETED' and s.result == 'SUCCESS'
                   for s in try_results.values())

    @staticmethod
    def some_failed(try_results):
        return any(s.result == 'FAILURE' for s in try_results.values())
