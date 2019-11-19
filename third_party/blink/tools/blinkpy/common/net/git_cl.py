# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An interface to git-cl.

The git-cl tool is responsible for communicating with Gerrit and Buildbucket to
manage changelists and try jobs associated with them.
"""

import collections
import json
import logging
import re

from blinkpy.common.checkout.git import Git
from blinkpy.common.net.results_fetcher import Build, filter_latest_builds
from blinkpy.common.net.luci_auth import LuciAuth


_log = logging.getLogger(__name__)

# A refresh token may be needed for some commands, such as git cl try,
# in order to authenticate with buildbucket.
_COMMANDS_THAT_TAKE_REFRESH_TOKEN = ('try',)

# These characters always appear at the beginning of the SearchBuilds response
# from BuildBucket.
SEARCHBUILDS_RESPONSE_PREFIX = ")]}'"


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

    @staticmethod
    def from_bb_status(bb_status):
        """Converts a buildbucket status into a TryJobStatus object."""
        assert bb_status in ('SCHEDULED', 'STARTED', 'SUCCESS', 'FAILURE', 'INFRA_FAILURE', 'CANCELLED')
        if bb_status in ('SCHEDULED', 'STARTED'):
            return TryJobStatus(bb_status, None)
        else:
            # Map result INFRA_FAILURE to FAILURE to avoid introducing a new
            # result, and it amounts to the same thing anyway.
            return TryJobStatus('COMPLETED',
                                'FAILURE' if bb_status == 'INFRA_FAILURE' else bb_status)


class GitCL(object):

    def __init__(self, host, auth_refresh_token_json=None, cwd=None):
        self._host = host
        self._auth_refresh_token_json = auth_refresh_token_json
        self._cwd = cwd
        self._git_executable_name = Git.find_executable_name(host.executive, host.platform)

    def run(self, args):
        """Runs git-cl with the given arguments and returns the output.

        Args:
            args: A list of arguments passed to `git cl`.

        Returns:
            A string (the output from git-cl).
        """
        command = [self._git_executable_name, 'cl'] + args
        if self._auth_refresh_token_json and args[0] in _COMMANDS_THAT_TAKE_REFRESH_TOKEN:
            command += ['--auth-refresh-token-json', self._auth_refresh_token_json]
        # Suppress the stderr of git-cl because git-cl will show a warning when
        # running on Swarming bots with local git cache.
        return self._host.executive.run_command(
            command, cwd=self._cwd, return_stderr=False, ignore_stderr=True)

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
            # Buckets are required by `git cl try`. When no bucket is specified,
            # use the default bucket.
            command.extend(['-B', bucket or 'luci.chromium.try'])
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

    def _get_latest_patchset(self):
        return self.run(['status', '--field=patch']).strip()

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
            issue_number = self.get_issue_number()
            try_job_results = self.latest_try_jobs(issue_number, cq_only=cq_only)
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
            _log.debug('CL status is: %s', status)
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

    def latest_try_jobs(
            self, issue_number=None, builder_names=None, cq_only=False, patchset=None):
        """Fetches a dict of Build to TryJobStatus for the latest try jobs.

        This variant fetches try job data from buildbucket directly.

        This includes jobs that are not yet finished and builds with infra
        failures, so if a build is in this list, that doesn't guarantee that
        there are results.

        Args:
            issue_number: The git cl/issue number we're working with.
            builder_names: Optional list of builders used to filter results.
            cq_only: If True, only include CQ jobs.
            patchset: If given, use this patchset instead of the latest.

        Returns:
            A dict mapping Build objects to TryJobStatus objects, with
            only the latest jobs included.
        """
        if not issue_number:
            issue_number = self.get_issue_number()
        return self.filter_latest(
            self.try_job_results(
                issue_number, builder_names, cq_only=cq_only, patchset=patchset))

    @staticmethod
    def filter_latest(try_results):
        """Returns the latest entries from from a Build to TryJobStatus dict."""
        if try_results is None:
            return None
        latest_builds = filter_latest_builds(try_results.keys())
        return {b: s for b, s in try_results.items() if b in latest_builds}

    def try_job_results(
            self, issue_number=None, builder_names=None, cq_only=False, patchset=None):
        """Returns a dict mapping Build objects to TryJobStatus objects."""
        if not issue_number:
            issue_number = self.get_issue_number()
        raw_results_json = self.fetch_raw_try_job_results(issue_number, patchset)
        build_to_status = {}
        if 'builds' not in raw_results_json:
            return build_to_status
        for build in raw_results_json['builds']:
            builder_name = build['builder']['builder']
            if builder_names and builder_name not in builder_names:
                continue
            is_cq = 'tags' in build and {'key': 'user_agent', 'value': 'cq'} in build['tags']
            is_experimental = 'tags' in build and {'key': 'cq_experimental', 'value': 'true'} in build['tags']
            if cq_only and not (is_cq and not is_experimental):
                continue
            build_number = build.get('number')
            status = build['status']
            build_to_status[Build(builder_name, build_number)] = TryJobStatus.from_bb_status(status)
        return build_to_status

    def fetch_raw_try_job_results(self, issue_number, patchset=None):
        """Gets try job results for the specified CL from buildbucket.

        This uses the SearchBuilds rpc format specified in
        https://cs.chromium.org/chromium/infra/go/src/go.chromium.org/luci/buildbucket/proto/rpc.proto

        The response is a list of dicts of the following form:
        {
            "builds": [
                {
                    "status": <status>
                    "builder": {
                        "builder": <builder_name>
                    },
                    "number": <build_number>,
                    "tags": [
                        {
                            "key": <tag key>
                            "value": <tag value>
                        },
                        ... more tags
                    ]
                },
                ... more builds
        }

        This method returns the JSON representation of the above response.
        """
        if not patchset:
            patchset = self._get_latest_patchset()

        luci_token = LuciAuth(self._host).get_access_token()
        hed = {
            'Authorization': 'Bearer ' + luci_token,
            'Accept': 'application/json',
            'Content-Type': 'application/json',
        }
        data = {
            'predicate': {
                'gerritChanges': [
                    {
                        'host': 'chromium-review.googlesource.com',
                        'project': 'chromium/src',
                        'change': issue_number,
                        'patchset': patchset
                    }
                ]
            },
            'fields': 'builds.*.builder.builder,builds.*.status,builds.*.tags,builds.*.number'
        }
        url = 'https://cr-buildbucket.appspot.com/prpc/buildbucket.v2.Builds/SearchBuilds'
        req_body = json.dumps(data)
        _log.debug("Sending SearchBuilds request. Url: %s with Body: %s" % (url, req_body))
        response = self._host.web.request('POST', url, data=req_body, headers=hed)
        if response.getcode() == 200:
            response_body = response.read()
            if response_body.startswith(SEARCHBUILDS_RESPONSE_PREFIX):
                response_body = response_body[len(SEARCHBUILDS_RESPONSE_PREFIX):]
            return json.loads(response_body)

        _log.error("Failed to fetch tryjob results from buildbucket (status=%s)" % response.status)
        _log.debug("Full SearchBuilds response: %s" % str(response))
        return None


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
