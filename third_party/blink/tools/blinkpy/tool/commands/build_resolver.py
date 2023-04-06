# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import logging
import re
from concurrent.futures import Executor
from typing import Collection, Dict, Optional, Tuple

from requests.exceptions import RequestException

from blinkpy.common import exit_codes
from blinkpy.common.net.rpc import Build
from blinkpy.common.net.web import Web
from blinkpy.common.net.git_cl import BuildStatuses, GitCL, TryJobStatus

_log = logging.getLogger(__name__)


class UnresolvedBuildException(Exception):
    """Exception raised when the tool should halt because of unresolved builds.

    Note that this is not necessarily an error that needs action (e.g.,
    waiting for unfinished tryjobs).
    """


class BuildResolver:
    """Resolve builder names and build numbers into statuses.

    This resolver fetches build information from Buildbucket.
    """
    # Pseudo-status to indicate a build was triggered by this command run.
    # More specific than the 'SCHEDULED' Buildbucket status.
    TRIGGERED = TryJobStatus('TRIGGERED', None)

    # Pseudo-status to indicate a try builder is missing a build and cannot
    # get one from triggering a job.
    MISSING = TryJobStatus('MISSING', None)

    # Build fields required by `_build_statuses_from_responses`.
    _build_fields = [
        'id',
        'number',
        'builder.builder',
        'builder.bucket',
        'status',
        'steps.*.name',
        'steps.*.logs.*.name',
        'steps.*.logs.*.view_url',
    ]

    def __init__(self,
                 web: Web,
                 git_cl: GitCL,
                 io_pool: Optional[Executor] = None,
                 can_trigger_jobs: bool = False):
        self._web = web
        self._git_cl = git_cl
        self._io_pool = io_pool
        self._can_trigger_jobs = can_trigger_jobs

    def _builder_predicate(self, build: Build) -> Dict[str, str]:
        return {
            'project': 'chromium',
            'bucket': build.bucket,
            'builder': build.builder_name,
        }

    def _build_statuses_from_responses(self, raw_builds) -> BuildStatuses:
        raw_builds = list(raw_builds)
        map_fn = self._io_pool.map if self._io_pool else map
        statuses = map_fn(self._status_if_interrupted, raw_builds)
        return {
            Build(build['builder']['builder'], build['number'], build['id'],
                  build['builder']['bucket']): status
            for build, status in zip(raw_builds, statuses)
        }

    def resolve_builds(self,
                       builds: Collection[Build],
                       patchset: Optional[int] = None) -> BuildStatuses:
        """Resolve builders (maybe with build numbers) into statuses.

        Arguments:
            builds: Builds to fetch. If the build number is not specified, the
                build number is inferred:
                  * For try builders, the build for the specified patchset on
                    the current CL. If such a build does not exist, a build can
                    be triggered.
                  * For CI builders, the latest failing build.
                Multiple builds from the same builder are allowed.
            patchset: Patchset that try build results should be fetched from.
        """
        try_builders_to_infer = set()
        for build in builds:
            if build.build_number:
                self._git_cl.bb_client.add_get_build_req(
                    build,
                    build_fields=self._build_fields)
            elif build.bucket == 'try':
                try_builders_to_infer.add(build.builder_name)
            else:
                predicate = {
                    'builder': self._builder_predicate(build),
                    'status': 'FAILURE'
                }
                self._git_cl.bb_client.add_search_builds_req(
                    predicate, build_fields=self._build_fields, count=1)

        build_statuses = {}
        # Handle implied tryjobs first, since there are more failure modes.
        if try_builders_to_infer:
            try_build_statuses = self.fetch_or_trigger_try_jobs(
                try_builders_to_infer, patchset)
            build_statuses.update(try_build_statuses)
            # Re-request completed try builds so that the resolver can check
            # for interrupted steps.
            for build, (status, _) in try_build_statuses.items():
                if build.build_number and status == 'COMPLETED':
                    self._git_cl.bb_client.add_get_build_req(
                        build, build_fields=self._build_fields)
        # Find explicit or CI builds.
        build_statuses.update(
            self._build_statuses_from_responses(
                self._git_cl.bb_client.execute_batch()))
        if build_statuses:
            self.log_builds(build_statuses)
        if self.TRIGGERED in build_statuses.values():
            raise UnresolvedBuildException(
                'Once all pending try jobs have finished, '
                'please re-run the tool to fetch new results.')
        return build_statuses

    def _status_if_interrupted(self, raw_build) -> TryJobStatus:
        """Map non-browser-related failures to an infrastructue failure status.

        Such failures include shard-level timeouts and early exits caused by
        exceeding the failure threshold. These failures are opaque to LUCI, but
        can be discovered from `run_web_tests.py` exit code conventions.
        """
        # TODO(crbug.com/1123077): After the switch to wptrunner, stop checking
        # the `blink_wpt_tests` step.
        run_web_tests_pattern = re.compile(
            r'[\w_-]*blink_(web|wpt)_tests.*\(with patch\)[^|]*')
        for step in raw_build.get('steps', []):
            if run_web_tests_pattern.fullmatch(step['name']):
                summary = self._fetch_swarming_summary(step)
                shards = (summary or {}).get('shards', [])
                if any(map(_shard_interrupted, shards)):
                    return TryJobStatus.from_bb_status('INFRA_FAILURE')
        return TryJobStatus.from_bb_status(raw_build['status'])

    def _fetch_swarming_summary(self,
                                step,
                                log_name: str = 'chromium_swarming.summary'):
        for log in step.get('logs', []):
            if log['name'] == log_name:
                with contextlib.suppress(RequestException):
                    params = {'format': 'raw'}
                    return self._web.session.get(log['viewUrl'],
                                                 params=params).json()
        return None

    def fetch_or_trigger_try_jobs(
            self,
            builders: Collection[str],
            patchset: Optional[int] = None,
    ) -> BuildStatuses:
        """Fetch or trigger try jobs for the current CL.

        Arguments:
            builders: Try builder names.
            patchset: Patchset that build results should be fetched from.
                Defaults to the latest patchset.

        Raises:
            UnresolvedBuildException: If the CL issue number is not set or no
                try jobs are available but try jobs cannot be triggered.
        """
        if not self._git_cl.get_issue_number().isdigit():
            raise UnresolvedBuildException(
                'No issue number for current branch.')
        build_statuses = self._git_cl.latest_try_jobs(builder_names=builders,
                                                      patchset=patchset)
        if not build_statuses and not self._can_trigger_jobs:
            raise UnresolvedBuildException(
                "Aborted: no try jobs and '--no-trigger-jobs' or '--dry-run' "
                'passed.')

        builders_without_results = set(builders) - {
            build.builder_name
            for build in build_statuses
        }
        placeholder_status = self.MISSING
        if self._can_trigger_jobs and builders_without_results:
            self._git_cl.trigger_try_jobs(builders_without_results)
            placeholder_status = self.TRIGGERED
        for builder in builders_without_results:
            build_statuses[Build(builder)] = placeholder_status
        return build_statuses

    def log_builds(self, build_statuses: BuildStatuses):
        """Log builds in a tabular format."""
        self._warn_about_infra_failures(build_statuses)
        finished_builds = {
            build: result or '--'
            for build, (status, result) in build_statuses.items()
            if status == 'COMPLETED'
        }
        if len(finished_builds) == len(build_statuses):
            _log.info('All builds finished.')
            return
        if finished_builds:
            _log.info('Finished builds:')
            self._log_build_statuses(finished_builds)
        else:
            _log.info('No finished builds.')
        unfinished_builds = {
            build: status
            for build, (status, _) in build_statuses.items()
            if build not in finished_builds and status != 'MISSING'
        }
        if unfinished_builds:
            _log.info('Scheduled or started builds:')
            self._log_build_statuses(unfinished_builds)

    def _warn_about_infra_failures(self, build_statuses: BuildStatuses):
        builds_with_infra_failures = GitCL.filter_infra_failed(build_statuses)
        if builds_with_infra_failures:
            _log.warning('Some builds have infrastructure failures:')
            for build in sorted(builds_with_infra_failures,
                                key=_build_sort_key):
                _log.warning('  "%s" build %s', build.builder_name,
                             str(build.build_number or '--'))
            _log.warning('Examples of infrastructure failures include:')
            _log.warning('  * Shard terminated the harness after timing out.')
            _log.warning('  * Harness exited early due to '
                         'excessive unexpected failures.')
            _log.warning('  * Build failed on a non-test step.')
            _log.warning('Please consider retrying the failed builders or '
                         'giving the builders more shards.')
            _log.warning(
                'See https://chromium.googlesource.com/chromium/src/+/HEAD/'
                'docs/testing/web_test_expectations.md#handle-bot-timeouts')

    def _log_build_statuses(self, build_statuses: BuildStatuses):
        template = '  %-20s %-7s %-9s %-6s'
        _log.info(template, 'BUILDER', 'NUMBER', 'STATUS', 'BUCKET')
        for build in sorted(build_statuses, key=_build_sort_key):
            _log.info(template, build.builder_name,
                      str(build.build_number or '--'), build_statuses[build],
                      build.bucket)


def _build_sort_key(build: Build) -> Tuple[str, int]:
    return (build.builder_name, build.build_number or 0)


def _shard_interrupted(shard) -> bool:
    return int(shard.get('exit_code', 0)) in exit_codes.ERROR_CODES
