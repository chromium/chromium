#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs tests with Xvfb and Openbox or Weston on Linux and normally on other
   platforms."""

import os
import os.path
import psutil
import random
import re
import signal
import subprocess
import sys
import threading
import time
import test_env


class _XvfbProcessError(Exception):
  """Exception raised when Xvfb cannot start."""
  pass


class _WestonProcessError(Exception):
  """Exception raised when Weston cannot start."""
  pass


def kill(proc, name, timeout_in_seconds=10):
  """Tries to kill |proc| gracefully with a timeout for each signal."""
  if not proc:
    return

  proc.terminate()
  thread = threading.Thread(target=proc.wait)
  thread.start()

  thread.join(timeout_in_seconds)
  if thread.is_alive():
    print >> sys.stderr, '%s running after SIGTERM, trying SIGKILL.' % name
    proc.kill()

  thread.join(timeout_in_seconds)
  if thread.is_alive():
    print >> sys.stderr, \
      '%s running after SIGTERM and SIGKILL; good luck!' % name


def launch_dbus(env):
  """Starts a DBus session.

  Works around a bug in GLib where it performs operations which aren't
  async-signal-safe (in particular, memory allocations) between fork and exec
  when it spawns subprocesses. This causes threads inside Chrome's browser and
  utility processes to get stuck, and this harness to hang waiting for those
  processes, which will never terminate. This doesn't happen on users'
  machines, because they have an active desktop session and the
  DBUS_SESSION_BUS_ADDRESS environment variable set, but it can happen on
  headless environments. This is fixed by glib commit [1], but this workaround
  will be necessary until the fix rolls into Chromium's CI.

  [1] f2917459f745bebf931bccd5cc2c33aa81ef4d12

  Modifies the passed in environment with at least DBUS_SESSION_BUS_ADDRESS and
  DBUS_SESSION_BUS_PID set.

  Returns the pid of the dbus-daemon if started, or None otherwise.
  """
  if 'DBUS_SESSION_BUS_ADDRESS' in os.environ:
    return
  try:
    dbus_output = subprocess.check_output(['dbus-launch'], env=env).split('\n')
    for line in dbus_output:
      m = re.match(r'([^=]+)\=(.+)', line)
      if m:
        env[m.group(1)] = m.group(2)
    return int(env['DBUS_SESSION_BUS_PID'])
  except (subprocess.CalledProcessError, OSError, KeyError, ValueError) as e:
    print 'Exception while running dbus_launch: %s' % e


# TODO(crbug.com/949194): Encourage setting flags to False.
def run_executable(
    cmd, env, stdoutfile=None, use_openbox=True, use_xcompmgr=True):
  """Runs an executable within Weston or Xvfb on Linux or normally on other
     platforms.

  The method sets SIGUSR1 handler for Xvfb to return SIGUSR1
  when it is ready for connections.
  https://www.x.org/archive/X11R7.5/doc/man/man1/Xserver.1.html under Signals.

  Args:
    cmd: Command to be executed.
    env: A copy of environment variables, "DISPLAY" and
      "_CHROMIUM_INSIDE_XVFB" will be set if Xvfb is used. "WAYLAND_DISPLAY"
      will be set if Weston is used.
    stdoutfile: If provided, symbolization via script is disabled and stdout
      is written to this file as well as to stdout.
    use_openbox: A flag to use openbox process.
      Some ChromeOS tests need a window manager.
    use_xcompmgr: A flag to use xcompmgr process.
      Some tests need a compositing wm to make use of transparent visuals.

  Returns:
    the exit code of the specified commandline, or 1 on failure.
  """

  # It might seem counterintuitive to support a --no-xvfb flag in a script
  # whose only job is to start xvfb, but doing so allows us to consolidate
  # the logic in the layers of buildbot scripts so that we *always* use
  # xvfb by default and don't have to worry about the distinction, it
  # can remain solely under the control of the test invocation itself.
  use_xvfb = True
  if '--no-xvfb' in cmd:
    use_xvfb = False
    cmd.remove('--no-xvfb')

  # Tests that run on Linux platforms with Ozone/Wayland backend require
  # a Weston instance. However, it is also required to disable xvfb so
  # that Weston can run in a pure headless environment.
  use_weston = False
  if '--use-weston' in cmd:
    if use_xvfb:
      print >> sys.stderr, 'Unable to use Weston with xvfb.'
      return 1
    use_weston = True
    cmd.remove('--use-weston')

  if sys.platform == 'linux2' and use_xvfb:
    return _run_with_xvfb(cmd, env, stdoutfile, use_openbox, use_xcompmgr)
  elif use_weston:
    return _run_with_weston(cmd, env, stdoutfile)
  else:
    return test_env.run_executable(cmd, env, stdoutfile)


def _run_with_xvfb(cmd, env, stdoutfile, use_openbox, use_xcompmgr):
  env['_CHROMIUM_INSIDE_XVFB'] = '1'
  openbox_proc = None
  xcompmgr_proc = None
  xvfb_proc = None
  xwmstartupcheck_proc = None
  xvfb_ready = MutableBoolean()
  def set_xvfb_ready(*_):
    xvfb_ready.setvalue(True)

  try:
    signal.signal(signal.SIGTERM, raise_xvfb_error)
    signal.signal(signal.SIGINT, raise_xvfb_error)

    # Due to race condition for display number, Xvfb might fail to run.
    # If it does fail, try again up to 10 times, similarly to xvfb-run.
    for _ in range(10):
      xvfb_ready.setvalue(False)
      display = find_display()

      # Sets SIGUSR1 to ignore for Xvfb to signal current process
      # when it is ready. Due to race condition, USR1 signal could be sent
      # before the process resets the signal handler, we cannot rely on
      # signal handler to change on time.
      signal.signal(signal.SIGUSR1, signal.SIG_IGN)
      xvfb_proc = subprocess.Popen(
          ['Xvfb', display, '-screen', '0', '1280x800x24', '-ac',
           '-nolisten', 'tcp', '-dpi', '96', '+extension', 'RANDR'],
          stderr=subprocess.STDOUT, env=env)
      signal.signal(signal.SIGUSR1, set_xvfb_ready)
      for _ in range(10):
        time.sleep(.1)  # gives Xvfb time to start or fail.
        if xvfb_ready.getvalue() or xvfb_proc.poll() is not None:
          break  # xvfb sent ready signal, or already failed and stopped.

      if xvfb_proc.poll() is None:
        break  # xvfb is running, can proceed.
    if xvfb_proc.poll() is not None:
      raise _XvfbProcessError('Failed to start after 10 tries')

    env['DISPLAY'] = display

    dbus_pid = launch_dbus(env)

    if use_openbox:
      # This is not ideal, but x11_unittests require that (other X11 tests have
      # a race with the openbox as well, but they take more time to initialize.
      # And thus, they do no time out compate to the x11_unittests that are
      # quick enough to start up before openbox is ready.
      # TODO(dpranke): remove this nasty hack once the test() template is
      # reworked.
      wait_for_openbox = False
      wait_openbox_program = './xwmstartupcheck'
      if not os.path.isfile(wait_openbox_program):
        wait_for_openbox = False
      # Creates a dummy window that waits for a ReparentNotify event that is
      # sent whenever Openbox WM starts. Must be started before the OpenBox WM
      # so that it does not miss the event. This helper program is located in
      # the current build directory. The program terminates automatically after
      # 30 seconds of waiting for the event.
      if wait_for_openbox:
        xwmstartupcheck_proc = subprocess.Popen(
            wait_openbox_program, stderr=subprocess.STDOUT, env=env)

      openbox_proc = subprocess.Popen(
          ['openbox', '--sm-disable'], stderr=subprocess.STDOUT, env=env)

      # Wait until execution is done. Does not block if the process has already
      # been terminated. In that case, it's safe to read the return value.
      if wait_for_openbox:
        xwmstartupcheck_proc.wait()
        if xwmstartupcheck_proc.returncode is not 0:
          raise _XvfbProcessError('Failed to get OpenBox up.')

    if use_xcompmgr:
      xcompmgr_proc = subprocess.Popen(
          'xcompmgr', stderr=subprocess.STDOUT, env=env)

    return test_env.run_executable(cmd, env, stdoutfile)
  except OSError as e:
    print >> sys.stderr, 'Failed to start Xvfb or Openbox: %s' % str(e)
    return 1
  except _XvfbProcessError as e:
    print >> sys.stderr, 'Xvfb fail: %s' % str(e)
    return 1
  finally:
    kill(openbox_proc, 'openbox')
    kill(xcompmgr_proc, 'xcompmgr')
    kill(xvfb_proc, 'Xvfb')

    # dbus-daemon is not a subprocess, so we can't SIGTERM+waitpid() on it.
    # To ensure it exits, use SIGKILL which should be safe since all other
    # processes that it would have been servicing have exited.
    if dbus_pid:
      os.kill(dbus_pid, signal.SIGKILL)


# TODO(https://crbug.com/1060466): Write tests.
def _run_with_weston(cmd, env, stdoutfile):
  weston_proc = None

  try:
    signal.signal(signal.SIGTERM, raise_weston_error)
    signal.signal(signal.SIGINT, raise_weston_error)

    # Set $XDG_RUNTIME_DIR if it is not set.
    _set_xdg_runtime_dir(env)

    weston_proc_display = None
    for _ in range(10):
      # Weston is compiled along with the Ozone/Wayland platform, and is
      # fetched as data deps. Thus, run it from the current directory.
      #
      # Weston is used with the following flags:
      # 1) --backend=headless-backend.so - runs Weston in a headless mode
      # that does not require a real GPU card.
      # 2) --idle-time=0 - disables idle timeout, which prevents Weston
      # to enter idle state. Otherwise, Weston stops to send frame callbacks,
      # and tests start to time out (this typically happens after 300 seconds -
      # the default time after which Weston enters the idle state).
      # 3) --width && --height set size of a virtual display: we need to set
      # an adequate size so that tests can have more room for managing size
      # of windows.
      weston_proc = subprocess.Popen(
         ('./weston', '--backend=headless-backend.so', '--idle-time=0',
          '--width=1024', '--height=768'),
         stderr=subprocess.STDOUT, env=env)

      # Get the $WAYLAND_DISPLAY set by Weston and pass it to the test launcher.
      # Please note that this env variable is local for the process. That's the
      # reason we have to read it from Weston separately.
      weston_proc_display = _get_display_from_weston(weston_proc.pid)
      if weston_proc_display is not None:
        break # Weston could launch and we found the display.

    # If we couldn't find the display after 10 tries, raise an exception.
    if weston_proc_display is None:
      raise _WestonProcessError('Failed to start Weston.')
    env['WAYLAND_DISPLAY'] = weston_proc_display
    return test_env.run_executable(cmd, env, stdoutfile)
  except OSError as e:
    print >> sys.stderr, 'Failed to start Weston: %s' % str(e)
    return 1
  except _WestonProcessError as e:
    print >> sys.stderr, 'Weston fail: %s' % str(e)
    return 1
  finally:
    kill(weston_proc, 'weston')


def _get_display_from_weston(weston_proc_pid):
  """Retrieves $WAYLAND_DISPLAY set by Weston.

  Searches for the child "weston-desktop-shell" process, takes its
  environmental variables, and returns $WAYLAND_DISPLAY variable set
  by that process. If the variable is not set, tries up to 10 times
  and then gives up.

  Args:
    weston_proc_pid: The process of id of the main Weston process.

  Returns:
    the display set by Wayland, which clients can use to connect to.

  TODO(https://crbug.com/1060469): This is potentially error prone
  function. See the bug for further details.
  """

  # Try 10 times as it is not known when Weston spawn child desktop shell
  # process.
  for _ in range(10):
    # gives weston time to start or fail.
    time.sleep(.05)
    # Take the parent process.
    parent = psutil.Process(weston_proc_pid)
    if parent is None:
      break # The process is not found. Give up.

    # Traverse through all the children processes and find the
    # "weston-desktop-shell" process that sets local to process env variables
    # including the $WAYLAND_DISPLAY.
    children = parent.children(recursive=True)
    for process in children:
      if process.name() == "weston-desktop-shell":
        weston_proc_display = process.environ().get('WAYLAND_DISPLAY')
        # If display is set, Weston could start successfully and we can use
        # that display for Wayland connection in Chromium.
        if weston_proc_display is not None:
          return weston_proc_display
  return None


class MutableBoolean(object):
  """Simple mutable boolean class. Used to be mutated inside an handler."""

  def __init__(self):
    self._val = False

  def setvalue(self, val):
    assert isinstance(val, bool)
    self._val = val

  def getvalue(self):
    return self._val


def raise_xvfb_error(*_):
  raise _XvfbProcessError('Terminated')


def raise_weston_error(*_):
  raise _WestonProcessError('Terminated')


def find_display():
  """Iterates through X-lock files to find an available display number.

  The lower bound follows xvfb-run standard at 99, and the upper bound
  is set to 119.

  Returns:
    A string of a random available display number for Xvfb ':{99-119}'.

  Raises:
    _XvfbProcessError: Raised when displays 99 through 119 are unavailable.
  """

  available_displays = [
      d for d in range(99, 120)
      if not os.path.isfile('/tmp/.X{}-lock'.format(d))
  ]
  if available_displays:
    return ':{}'.format(random.choice(available_displays))
  raise _XvfbProcessError('Failed to find display number')


def _set_xdg_runtime_dir(env):
  """Sets the $XDG_RUNTIME_DIR variable if it hasn't been set before."""
  runtime_dir = env.get('XDG_RUNTIME_DIR')
  if not runtime_dir:
    runtime_dir = '/tmp/xdg-tmp-dir/'
    if not os.path.exists(runtime_dir):
      os.makedirs(runtime_dir, 0700)
    env['XDG_RUNTIME_DIR'] = runtime_dir


def main():
  usage = 'Usage: xvfb.py [command [--no-xvfb or --use-weston] args...]'
  if len(sys.argv) < 2:
    print >> sys.stderr, usage
    return 2

  # If the user still thinks the first argument is the execution directory then
  # print a friendly error message and quit.
  if os.path.isdir(sys.argv[1]):
    print >> sys.stderr, (
        'Invalid command: \"%s\" is a directory' % sys.argv[1])
    print >> sys.stderr, usage
    return 3

  return run_executable(sys.argv[1:], os.environ.copy())


if __name__ == '__main__':
  sys.exit(main())
