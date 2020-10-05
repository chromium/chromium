# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import multiprocessing

from .package_initializer import package_initializer


class TaskQueue(object):
    """
    Represents a task queue to run tasks with using a worker pool.  Scheduled
    tasks will be executed in parallel.
    """

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
            self._pool = multiprocessing.Pool(self._pool_size,
                                              package_initializer().init)
        self._requested_tasks = []  # List of (func, args, kwargs)
        self._worker_tasks = []  # List of multiprocessing.pool.AsyncResult
        self._did_run = False

    def post_task(self, func, *args, **kwargs):
        """
        Schedules a new task to be executed when |run| method is invoked.  This
        method does not kick any execution, only puts a new task in the queue.
        """
        assert not self._did_run
        self._requested_tasks.append((func, args, kwargs))

    def run(self, report_progress=None):
        """
        Executes all scheduled tasks.

        Args:
            report_progress: A callable that takes two arguments, total number
                of worker tasks and number of completed worker tasks.
        """
        assert report_progress is None or callable(report_progress)
        assert not self._did_run
        assert not self._worker_tasks
        self._did_run = True

        if self._single_process:
            self._run_in_sequence(report_progress)
        else:
            self._run_in_parallel(report_progress)

    def _run_in_sequence(self, report_progress):
        for index, task in enumerate(self._requested_tasks):
            func, args, kwargs = task
            report_progress(len(self._requested_tasks), index)
            apply(func, args, kwargs)
        report_progress(len(self._requested_tasks), len(self._requested_tasks))

    def _run_in_parallel(self, report_progress):
        for task in self._requested_tasks:
            func, args, kwargs = task
            self._worker_tasks.append(
                self._pool.apply_async(func, args, kwargs))
        self._pool.close()

        def report_worker_task_progress():
            if not report_progress:
                return
            done_count = reduce(
                lambda count, worker_task: count + bool(worker_task.ready()),
                self._worker_tasks, 0)
            report_progress(len(self._worker_tasks), done_count)

        timeout_in_sec = 1
        while True:
            report_worker_task_progress()
            for worker_task in self._worker_tasks:
                if not worker_task.ready():
                    worker_task.wait(timeout_in_sec)
                    break
                if not worker_task.successful():
                    worker_task.get()  # Let |get()| raise an exception.
                    assert False
            else:
                break

        self._pool.join()
