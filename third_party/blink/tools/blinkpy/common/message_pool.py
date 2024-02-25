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
"""Module for handling messages and concurrency. This module follows the design
for multiprocessing.Pool and concurrency.futures.ProcessPoolExecutor, with the
following differences:

* Tasks are executed in stateful subprocesses via objects that implement the
  Worker interface - this allows the workers to share state across tasks.
* The pool provides an asynchronous event-handling interface so the caller
  may receive events as tasks are processed.

If you don't need these features, use multiprocessing.Pool or concurrency.futures
instead.
"""

import contextlib
import logging
import multiprocessing
import pickle
import queue
import six
import sys
import traceback
from typing import Any, Callable, Optional, Protocol

from blinkpy.common.host import Host
from blinkpy.common.system import stack_utils

_log = logging.getLogger(__name__)


class MessageHandler(Protocol):
    def handle(self, name: str, source: str, *args: Any) -> None:
        """Handle a message sent from the other end of a message queue.

        Arguments:
            name: An implementer-defined message name.
            source: The name of the caller.
            args: Any additional data associated with this message. The
                semantics are implementer-defined and may depend on `name`.
        """


class Worker(MessageHandler):
    """State maintained between tasks.

    Note: This object must be pickleable because it is instantiated in the
        parent process.
    """

    def start(self) -> None:
        """Initialize this object when the worker process starts (optional).

        Runs in the worker process.
        """

    def stop(self) -> None:
        """Clean up this object when the worker process exits (optional).

        Either the manager or worker process may call `stop()`, so resources
        created in `start()` instead of the constructor may not be available.
        You can use this to detect which process `stop()` is running in.
        """


def get(caller: MessageHandler,
        worker_factory: Callable[['_WorkerProcess'], Worker],
        num_workers: int,
        host: Optional[Host] = None):
    """Make a message pool object.

    Returns:
        A pool object that exposes a `run()` method that takes a list of
        pickleable data and distributes them to workers to process.
    """
    return _MessagePool(caller, worker_factory, num_workers, host)


class _MessagePool(object):
    def __init__(self, caller, worker_factory, num_workers, host=None):
        self._caller = caller
        self._worker_factory = worker_factory
        self._num_workers = num_workers
        self._workers = []
        self._workers_stopped = set()
        self._host = host
        self._name = 'manager'
        self._running_inline = (self._num_workers == 1)
        if self._running_inline:
            self._messages_to_worker = queue.Queue()
            self._messages_to_manager = queue.Queue()
        else:
            self._messages_to_worker = multiprocessing.Queue()
            self._messages_to_manager = multiprocessing.Queue()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self._close()
        return False

    def run(self, shards):
        """Posts a list of messages to the pool and waits for them to complete."""
        for message in shards:
            self._messages_to_worker.put(
                _Message(
                    self._name,
                    message[0],
                    message[1:],
                    from_user=True,
                    logs=()))

        for _ in range(self._num_workers):
            self._messages_to_worker.put(
                _Message(
                    self._name,
                    'stop',
                    message_args=(),
                    from_user=False,
                    logs=()))

        self.wait()

    def _start_workers(self):
        assert not self._workers
        self._workers_stopped = set()
        host = None
        if self._running_inline or self._can_pickle(self._host):
            host = self._host

        for worker_number in range(self._num_workers):
            worker = _WorkerProcess(host, self._messages_to_manager,
                                    self._messages_to_worker,
                                    self._worker_factory, worker_number,
                                    self._running_inline,
                                    self if self._running_inline else None,
                                    self._worker_log_level())
            self._workers.append(worker)
            worker.start()

    def _worker_log_level(self):
        log_level = logging.root.level
        for handler in logging.root.handlers:
            if handler.level != logging.NOTSET:
                if log_level == logging.NOTSET:
                    log_level = handler.level
                else:
                    log_level = min(log_level, handler.level)
        return log_level

    def wait(self):
        try:
            self._start_workers()
            if self._running_inline:
                self._workers[0].run()
                self._loop(block=False)
            else:
                self._loop(block=True)
        finally:
            self._close()

    def _close(self):
        for worker in self._workers:
            if worker.is_alive():
                worker.terminate()
        # Flush any remaining results or logs. There may be some stragglers
        # remaining if this pool context encountered an exception.
        with contextlib.suppress(Exception):
            self._loop(block=False)
        if not self._running_inline:
            # FIXME: This is a hack to get multiprocessing to not log tracebacks during shutdown :(.
            multiprocessing.util._exiting = True
            if self._messages_to_worker:
                self._messages_to_worker.close()
                self._messages_to_worker = None
            if self._messages_to_manager:
                self._messages_to_manager.close()
                self._messages_to_manager = None
        for worker in self._workers:
            if worker.is_alive():
                worker.kill()
                worker.join(1)
        self._workers.clear()

    def _log_messages(self, messages):
        for message in messages:
            logging.root.handle(message)

    def _handle_done(self, source):
        self._workers_stopped.add(source)

    @staticmethod
    def _handle_worker_exception(source, exception_type, exception_value, _):
        if exception_type == KeyboardInterrupt:
            raise exception_type(exception_value)
        raise WorkerException(str(exception_value))

    def _can_pickle(self, host):
        try:
            pickle.dumps(host)
            return True
        except Exception:
            return False

    def _loop(self, block):
        try:
            while True:
                if len(self._workers_stopped) == len(self._workers):
                    block = False
                message = self._messages_to_manager.get(block)
                self._log_messages(message.logs)
                if message.from_user:
                    self._caller.handle(message.name, message.src,
                                        *message.args)
                    continue
                method = getattr(self, '_handle_' + message.name)
                assert method, 'bad message %s' % repr(message)
                method(message.src, *message.args)
        except queue.Empty:
            pass


class WorkerException(BaseException):
    """Raised when we receive an unexpected/unknown exception from a worker."""


class _Message(object):
    def __init__(self, src, message_name, message_args, from_user, logs):
        self.src = src
        self.name = message_name
        self.args = message_args
        self.from_user = from_user
        self.logs = logs

    def __repr__(self):
        return '_Message(src=%s, name=%s, args=%s, from_user=%s, logs=%s)' % (
            self.src, self.name, self.args, self.from_user, self.logs)


class _WorkerProcess(multiprocessing.Process):
    def __init__(self, host, messages_to_manager, messages_to_worker,
                 worker_factory, worker_number, running_inline, manager,
                 log_level):
        super().__init__()
        self.host = host
        self.worker_number = worker_number
        self.name = 'worker/%d' % worker_number
        # Log messages are batched for the lifetime of each task. This prevents
        # output from different tasks from being interleaved.
        self.log_messages = []
        self.log_level = log_level
        self._running = False
        self._running_inline = running_inline
        self._manager = manager

        self._messages_to_manager = messages_to_manager
        self._messages_to_worker = messages_to_worker
        self._worker = worker_factory(self)
        self._logger = None
        self._log_handler = None

    def terminate(self):
        if self._worker:
            if hasattr(self._worker, 'stop'):
                self._worker.stop()
            self._worker = None
        # Sending `SIGTERM` is safe because the child process inherits the
        # managing process's handler, which simply re-raises `KeyboardInterrupt`
        # in the main thread (i.e., `_WorkerProcess.run()`).
        if self.is_alive():
            super().terminate()

    def _close(self):
        if self._log_handler and self._logger:
            self._logger.removeHandler(self._log_handler)
        self._log_handler = None
        self._logger = None
        # Need to give the queue's I/O thread time to put the 'done' message
        # into the underlying OS pipe. Without this, the message pool (manager)
        # may hang forever. See also: crrev.com/c/3723551.
        if not isinstance(self._messages_to_manager, queue.Queue):
            self._messages_to_manager.close()
            self._messages_to_manager.join_thread()

    def start(self):
        if not self._running_inline:
            super().start()

    def run(self):
        if not self.host:
            self.host = Host()
        if not self._running_inline:
            self._set_up_logging()

        worker = self._worker
        _log.debug('%s starting', self.name)
        self._running = True

        try:
            if hasattr(worker, 'start'):
                worker.start()
            while self._running:
                message = self._messages_to_worker.get()
                if message.from_user:
                    worker.handle(message.name, message.src, *message.args)
                    self._yield_to_manager()
                else:
                    assert message.name == 'stop', 'bad message %s' % repr(
                        message)
                    break

            _log.debug('%s exiting', self.name)
        except queue.Empty:
            assert False, '%s: ran out of messages in worker queue.' % self.name
        except KeyboardInterrupt:
            self._raise(sys.exc_info())
        except Exception:
            self._raise(sys.exc_info())
        finally:
            try:
                if hasattr(worker, 'stop'):
                    worker.stop()
            finally:
                self._post(name='done', args=(), from_user=False)
            self._close()

    def stop_running(self):
        self._running = False

    def post(self, name, *args):
        self._post(name, args, from_user=True)
        self._yield_to_manager()

    def _yield_to_manager(self):
        if self._running_inline:
            self._manager._loop(block=False)

    def _post(self, name, args, from_user):
        log_messages = self.log_messages
        self.log_messages = []
        self._messages_to_manager.put(
            _Message(self.name, name, args, from_user, log_messages))

    def _raise(self, exc_info):
        exception_type, exception_value, exception_traceback = exc_info
        if self._running_inline:
            six.reraise(exception_type, exception_value, exception_traceback)

        if exception_type == KeyboardInterrupt:
            _log.debug('%s: interrupted, exiting', self.name)
            stack_utils.log_traceback(_log.debug, exception_traceback)
        else:
            _log.error("%s: %s('%s') raised:",
                       self.name, exception_value.__class__.__name__,
                       str(exception_value))
            stack_utils.log_traceback(_log.error, exception_traceback)
        # Since tracebacks aren't picklable, send the extracted stack instead.
        stack = traceback.extract_tb(exception_traceback)
        self._post(
            name='worker_exception',
            args=(exception_type, exception_value, stack),
            from_user=False)

    def _set_up_logging(self):
        self._logger = logging.getLogger()

        # The unix multiprocessing implementation clones any log handlers into the child process,
        # so we remove them to avoid duplicate logging.
        for h in self._logger.handlers:
            self._logger.removeHandler(h)

        self._log_handler = _WorkerLogHandler(self)
        self._logger.addHandler(self._log_handler)
        self._logger.setLevel(self.log_level)


class _WorkerLogHandler(logging.Handler):
    def __init__(self, worker):
        super().__init__()
        self._worker = worker
        self.setLevel(worker.log_level)

    def emit(self, record):
        self._worker.log_messages.append(record)
