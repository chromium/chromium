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
from blinkpy.common.host import Host
from blinkpy.common.net.rpc import Build
from blinkpy.common.net.git_cl import (
    BuildStatus,
    BuildStatuses,
    CLRevisionID,
    GitCL,
)
from blinkpy.tool.grammar import pluralize
from blinkpy.w3c.gerrit import GerritAPI, OutputOption

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
    # Build fields required by `_build_statuses_from_responses`.
    _build_fields = [
        'id',
        'number',
        'builder.builder',
        'builder.bucket',
        'status',
        'output.properties',
        'steps.*.name',
        'steps.*.logs.*.name',
        'steps.*.logs.*.view_url',
    ]

    def __init__(self,
                 host: Host,
                 git_cl: GitCL,
                 io_pool: Optional[Executor] = None,
                 gerrit: Optional[GerritAPI] = None,
                 can_trigger_jobs: bool = False):
        self._web = host.web
        self._gerrit = gerrit or GerritAPI(host)
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
            for build, status in try_build_statuses.items():
                if build.build_number and status in BuildStatus.COMPLETED:
                    self._git_cl.bb_client.add_get_build_req(
                        build, build_fields=self._build_fields)
        # Find explicit or CI builds.
        build_statuses.update(
            self._build_statuses_from_responses(
                self._git_cl.bb_client.execute_batch()))
        if build_statuses:
            self.log_builds(build_statuses)
        if BuildStatus.TRIGGERED in build_statuses.values():
            raise UnresolvedBuildException(
                'Once all pending try jobs have finished, '
                'please re-run the tool to fetch new results.')
        return build_statuses

    def _status_if_interrupted(self, raw_build) -> BuildStatus:
        """Map non-browser-related failures to an infrastructue failure status.

        Such failures include shard-level timeouts and early exits caused by
        exceeding the failure threshold. These failures are opaque to LUCI, but
        can be discovered from `run_web_tests.py` exit code conventions.
        """
        # TODO(crbug.com/352762538):
        #  1. Fetch shard exit codes separately for each suite.
        #  2. Instead of coercing bad shards to `INFRA_FAILURE`, they should be
        #     interpreted to populate `WebTestResults.incomplete_reason`
        #     directly.
        run_web_tests_pattern = re.compile(
            r'[\w_-]*(webdriver|blink_(web|wpt))_tests.*\(with patch\)[^|]*')
        status = BuildStatus[raw_build['status']]
        if status is BuildStatus.FAILURE:
            output_props = raw_build.get('output', {}).get('properties', {})
            # Buildbucket's `FAILURE` status encompasses both normal test
            # failures (i.e., needs rebaseline) and unrelated compile or
            # result merge failures. To distinguish them, look at the failure
            # reason yielded by the recipe.
            failure_type = output_props.get('failure_type')
            try:
                status = BuildStatus[failure_type]
            except KeyError:
                status = BuildStatus.OTHER_FAILURE
        for step in raw_build.get('steps', []):
            if run_web_tests_pattern.fullmatch(step['name']):
                summary = self._fetch_swarming_summary(step)
                shards = (summary or {}).get('shards', [])
                if any(map(_shard_interrupted, shards)):
                    return BuildStatus.INFRA_FAILURE
        return status

    def _fetch_swarming_summary(self,
                                step,
                                log_name: str = 'chromium_swarming.summary'):
        # TODO(crbug.com/342409114): Use swarming v2 API to fetch shard status
        # and exit codes, not the potentially unstable
        # `chromium_swarming.summary` log.
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
                Defaults to the latest patchset that's not a trivial rebase or
                commit message edit.

        Raises:
            UnresolvedBuildException: If the CL issue number is not set or no
                try jobs are available but try jobs cannot be triggered.
        """
        issue_number = self._git_cl.get_issue_number()
        try:
            issue_number = int(issue_number)
        except ValueError as error:
            raise UnresolvedBuildException(
                'No issue number for current branch.') from error
        if not patchset:
            patchset = self.latest_nontrivial_patchset(issue_number)
        cl = CLRevisionID(int(issue_number), patchset)
        _log.info(f'Fetching status for {pluralize("build", len(builders))} '
                  f'from {cl}.')
        build_statuses = self._git_cl.latest_try_jobs(issue_number,
                                                      builder_names=builders,
                                                      patchset=patchset)
        if not build_statuses and not self._can_trigger_jobs:
            raise UnresolvedBuildException(
                "Aborted: no try jobs and '--no-trigger-jobs' or '--dry-run' "
                'passed.')

        builders_without_results = set(builders) - {
            build.builder_name
            for build in build_statuses
        }
        placeholder_status = BuildStatus.MISSING
        if self._can_trigger_jobs and builders_without_results:
            self._git_cl.trigger_try_jobs(builders_without_results)
            placeholder_status = BuildStatus.TRIGGERED
        for builder in builders_without_results:
            build_statuses[Build(builder)] = placeholder_status
        return build_statuses

    def latest_nontrivial_patchset(self, issue_number: int) -> int:
        output = OutputOption.ALL_REVISIONS | OutputOption.SKIP_DIFFSTAT
        cl = self._gerrit.query_cl(str(issue_number), output)
        # https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#revision-info
        for revision in sorted(cl.revisions.values(),
                               key=lambda rev: rev['_number'],
                               reverse=True):
            if revision.get('kind') == 'REWORK':
                return revision['_number']
        # This error shouldn't happen because the initial upload to Gerrit
        # should always be `REWORK`.
        raise UnresolvedBuildException(
            f'{CLRevisionID(issue_number)} has no nontrivial changes')

    def log_builds(self, build_statuses: BuildStatuses):
        """Log builds in a tabular format."""
        finished_builds = {
            build: status
            for build, status in build_statuses.items()
            if status in BuildStatus.COMPLETED
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
            for build, status in build_statuses.items() if
            build not in finished_builds and status is not BuildStatus.MISSING
        }
        if unfinished_builds:
            _log.info('Scheduled or started builds:')
            self._log_build_statuses(unfinished_builds)

    def _log_build_statuses(self, build_statuses: BuildStatuses):
        assert build_statuses
        builder_names = [build.builder_name for build in build_statuses]
        # Clamp to a minimum width to visually separate the `BUILDER` and
        # `NUMBER` columns.
        name_column_width = max(20, *map(len, builder_names))
        status_column_width = max(
            len(status.name) for status in build_statuses.values())
        template = (f'  %-{name_column_width}s %-7s '
                    f'%-{status_column_width}s %-6s')
        _log.info(template, 'BUILDER', 'NUMBER', 'STATUS', 'BUCKET')
        for build in sorted(build_statuses, key=_build_sort_key):
            _log.info(template, build.builder_name,
                      str(build.build_number or '--'),
                      build_statuses[build].name, build.bucket)


def _build_sort_key(build: Build) -> Tuple[str, int]:
    return (build.builder_name, build.build_number or 0)


def _shard_interrupted(shard) -> bool:
    if shard.get('state') not in {'COMPLETED', 'DEDUPED'}:
        return True
    return int(shard.get('exit_code', 0)) in exit_codes.ERROR_CODES
