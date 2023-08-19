# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import functools
import multiprocessing
import sys
import typing

from .package_initializer import package_initializer


class TaskQueue(object):
    """
    Represents a task queue to run tasks with using a worker pool.  Scheduled
    tasks will be executed in parallel.
    """

    @dataclasses.dataclass
    class _Task(object):
        workload: int
        func: callable
        args: typing.List[typing.Any]
        kwargs: typing.Dict[str, typing.Any]

    def __init__(self, single_process=False):
        """
        Args:
            single_process: True makes the instance will not create nor use a
                child process so that error messages will be easier to read.
                This is useful for debugging.
        """
        assert isinstance(single_process, bool)
        if single_process:
            self._single_process = True
            self._pool_size = 1
            self._pool = None
        else:
            self._single_process = False
            self._pool_size = multiprocessing.cpu_count()
            if sys.platform == 'win32':
                # TODO(crbug.com/1190269) - we can't use more than 56
                # cores on Windows or Python3 may hang.
                self._pool_size = min(self._pool_size, 56)
            self._pool = multiprocessing.Pool(self._pool_size,
                                              package_initializer().init)
        self._requested_tasks = []  # List of _Task
        self._did_run = False

    def post_task(self, func, *args, **kwargs):
        """
        Schedules a new task to be executed when |run| method is invoked. This
        method does not kick any execution, only puts a new task in the queue.
        This task will be scheduled without any workload hint, therefore will be
        queued in an arbitrary order. Use |post_task_with_workload| to influence
        the order this task is scheduled in.
        """
        self.post_task_with_workload(0, func, *args, **kwargs)

    def post_task_with_workload(self, workload, func, *args, **kwargs):
        """
        Schedules a new task to be executed when |run| method is invoked,
        including a hint regarding how long this task is expected to take. This
        method does not kick any execution, only puts a new task in the queue.
        Tasks with higher workload values will be run first. This tries to
        reduce the impact of long-running tasks on total wall time on
        multiprocessor systems.
        """
        assert not self._did_run
        self._requested_tasks.append(self._Task(workload, func, args, kwargs))

    def run(self, report_progress=None):
        """
        Executes all scheduled tasks.

        Args:
            report_progress: A callable that takes two arguments, total number
                of worker tasks and number of completed worker tasks.
        """
        assert report_progress is None or callable(report_progress)
        assert not self._did_run
        self._did_run = True

        self._requested_tasks = sorted(self._requested_tasks,
                                       key=lambda task: task.workload,
                                       reverse=True)

        if self._single_process:
            self._run_in_sequence(report_progress)
        else:
            self._run_in_parallel(report_progress)

    def _run_in_sequence(self, report_progress):
        for index, task in enumerate(self._requested_tasks):
            report_progress(len(self._requested_tasks), index)
            task.func(*task.args, **task.kwargs)
        report_progress(len(self._requested_tasks), len(self._requested_tasks))

    def _run_in_parallel(self, report_progress):
        worker_tasks = []  # List of multiprocessing.pool.AsyncResult
        for task in self._requested_tasks:
            worker_tasks.append(
                self._pool.apply_async(task.func, task.args, task.kwargs))
        self._pool.close()

        def report_worker_task_progress():
            if not report_progress:
                return
            done_count = functools.reduce(
                lambda count, worker_task: count + bool(worker_task.ready()),
                worker_tasks, 0)
            report_progress(len(worker_tasks), done_count)

        timeout_in_sec = 1
        while True:
            report_worker_task_progress()
            for worker_task in worker_tasks:
                if not worker_task.ready():
                    worker_task.wait(timeout_in_sec)
                    break
                if not worker_task.successful():
                    worker_task.get()  # Let |get()| raise an exception.
                    assert False
            else:
                break

        self._pool.join()
