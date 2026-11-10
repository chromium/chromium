# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import multiprocessing.util
import queue
import unittest

from blinkpy.common import message_pool


class _LiveWorker(object):
    """A worker process that is still alive when the pool closes."""

    def __init__(self):
        self.terminated = False

    def is_alive(self):
        return not self.terminated

    def terminate(self):
        self.terminated = True

    def kill(self):
        pass

    def join(self, timeout=None):
        pass


class _PartialMessageQueue(object):
    """A result queue that a killed worker left holding part of a message.

    Reading the real queue in that state blocks forever, so this fake counts
    reads instead.
    """

    def __init__(self):
        self.reads = 0

    def get(self, block=True, timeout=None):
        self.reads += 1
        raise queue.Empty

    def close(self):
        pass


class MessagePoolTest(unittest.TestCase):
    def setUp(self):
        # `_close()` sets this process-wide flag; don't leak it to other tests.
        self.addCleanup(
            setattr,
            multiprocessing.util,
            '_exiting',
            multiprocessing.util._exiting,
        )

    def test_close_skips_partial_message_after_terminating_worker(self):
        pool = message_pool.get(None, None, num_workers=2)
        # Replace the real result queue, which this test doesn't need.
        pool._messages_to_manager.close()
        results = _PartialMessageQueue()
        pool._messages_to_manager = results
        worker = _LiveWorker()
        pool._workers = [worker]

        pool._close()

        self.assertTrue(worker.terminated)
        self.assertEqual(
            results.reads, 0, '_close() read results after terminating'
        )
