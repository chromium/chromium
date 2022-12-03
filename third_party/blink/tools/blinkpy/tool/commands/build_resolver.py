# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from typing import Collection, Dict, Optional, Tuple

from blinkpy.common.net.rpc import Build
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
        'id', 'number', 'builder.builder', 'builder.bucket', 'status'
    ]

    def __init__(self,
                 git_cl: GitCL,
                 can_trigger_jobs: bool = False):
        self._git_cl = git_cl
        self._can_trigger_jobs = can_trigger_jobs

    def _builder_predicate(self, build: Build) -> Dict[str, str]:
        return {
            'project': 'chromium',
            'bucket': build.bucket,
            'builder': build.builder_name,
        }

    def _build_statuses_from_responses(self, raw_builds) -> BuildStatuses:
        return {
            Build(build['builder']['builder'], build['number'], build['id'],
                  build['builder']['bucket']):
            TryJobStatus.from_bb_status(build['status'])
            for build in raw_builds
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
            build_statuses.update(
                self.fetch_or_trigger_try_jobs(try_builders_to_infer,
                                               patchset))
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

    def fetch_or_trigger_try_jobs(self,
                                  builders: Collection[str],
                                  patchset: Optional[int] = None
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

    def _build_sort_key(self, build: Build) -> Tuple[str, int]:
        return (build.builder_name, build.build_number or 0)

    def _log_build_statuses(self, build_statuses: BuildStatuses):
        template = '  %-20s %-7s %-9s %-6s'
        _log.info(template, 'BUILDER', 'NUMBER', 'STATUS', 'BUCKET')
        for build in sorted(build_statuses, key=self._build_sort_key):
            _log.info(template, build.builder_name,
                      str(build.build_number or '--'), build_statuses[build],
                      build.bucket)
