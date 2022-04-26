#!/usr/bin/env vpython3
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs tests with Xvfb and Openbox or Weston on Linux and normally on other
   platforms."""

from __future__ import print_function

import copy
import os
import os.path
import random
import re
import signal
import subprocess
import sys
import threading
import time

import psutil

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
    print('%s running after SIGTERM, trying SIGKILL.\n' % name, file=sys.stderr)
    proc.kill()

  thread.join(timeout_in_seconds)
  if thread.is_alive():
    print('%s running after SIGTERM and SIGKILL; good luck!\n' % name,
          file=sys.stderr)


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
    dbus_output = subprocess.check_output(
        ['dbus-launch'], env=env).decode('utf-8').split('\n')
    for line in dbus_output:
      m = re.match(r'([^=]+)\=(.+)', line)
      if m:
        env[m.group(1)] = m.group(2)
    return int(env['DBUS_SESSION_BUS_PID'])
  except (subprocess.CalledProcessError, OSError, KeyError, ValueError) as e:
    print('Exception while running dbus_launch: %s' % e)


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
    env: A copy of environment variables. "DISPLAY" and will be set if Xvfb is
      used. "WAYLAND_DISPLAY" will be set if Weston is used.
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
      print('Unable to use Weston with xvfb.\n', file=sys.stderr)
      return 1
    use_weston = True
    cmd.remove('--use-weston')

  if sys.platform.startswith('linux') and use_xvfb:
    return _run_with_xvfb(cmd, env, stdoutfile, use_openbox, use_xcompmgr)
  elif use_weston:
    return _run_with_weston(cmd, env, stdoutfile)
  else:
    return test_env.run_executable(cmd, env, stdoutfile)


def _run_with_xvfb(cmd, env, stdoutfile, use_openbox, use_xcompmgr):
  openbox_proc = None
  openbox_ready = MutableBoolean()
  def set_openbox_ready(*_):
    openbox_ready.setvalue(True)

  xcompmgr_proc = None
  xvfb_proc = None
  xvfb_ready = MutableBoolean()
  def set_xvfb_ready(*_):
    xvfb_ready.setvalue(True)

  dbus_pid = None
  try:
    signal.signal(signal.SIGTERM, raise_xvfb_error)
    signal.signal(signal.SIGINT, raise_xvfb_error)

    # Before [1], the maximum number of X11 clients was 256.  After, the default
    # limit is 256 with a configurable maximum of 512.  On systems with a large
    # number of CPUs, the old limit of 256 may be hit for certain test suites
    # [2] [3], so we set the limit to 512 when possible.  This flag is not
    # available on Ubuntu 16.04 or 18.04, so a feature check is required.  Xvfb
    # does not have a '-version' option, so checking the '-help' output is
    # required.
    #
    # [1] d206c240c0b85c4da44f073d6e9a692afb6b96d2
    # [2] https://crbug.com/1187948
    # [3] https://crbug.com/1120107
    xvfb_help = subprocess.check_output(
      ['Xvfb', '-help'], stderr=subprocess.STDOUT).decode('utf8')

    # Due to race condition for display number, Xvfb might fail to run.
    # If it does fail, try again up to 10 times, similarly to xvfb-run.
    for _ in range(10):
      xvfb_ready.setvalue(False)
      display = find_display()

      xvfb_cmd = ['Xvfb', display, '-screen', '0', '1280x800x24', '-ac',
                  '-nolisten', 'tcp', '-dpi', '96', '+extension', 'RANDR']
      if '-maxclients' in xvfb_help:
        xvfb_cmd += ['-maxclients', '512']

      # Sets SIGUSR1 to ignore for Xvfb to signal current process
      # when it is ready. Due to race condition, USR1 signal could be sent
      # before the process resets the signal handler, we cannot rely on
      # signal handler to change on time.
      signal.signal(signal.SIGUSR1, signal.SIG_IGN)
      xvfb_proc = subprocess.Popen(xvfb_cmd, stderr=subprocess.STDOUT, env=env)
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
    # Set dummy variable for scripts.
    env['XVFB_DISPLAY'] = display

    dbus_pid = launch_dbus(env)

    if use_openbox:
      # Openbox will send a SIGUSR1 signal to the current process notifying the
      # script it has started up.
      current_proc_id = os.getpid()

      # The CMD that is passed via the --startup flag.
      openbox_startup_cmd = 'kill --signal SIGUSR1 %s' % str(current_proc_id)
      # Setup the signal handlers before starting the openbox instance.
      signal.signal(signal.SIGUSR1, signal.SIG_IGN)
      signal.signal(signal.SIGUSR1, set_openbox_ready)
      openbox_proc = subprocess.Popen(
          ['openbox', '--sm-disable', '--startup',
           openbox_startup_cmd], stderr=subprocess.STDOUT, env=env)

      for _ in range(10):
        time.sleep(.1)  # gives Openbox time to start or fail.
        if openbox_ready.getvalue() or openbox_proc.poll() is not None:
          break  # openbox sent ready signal, or failed and stopped.

      if openbox_proc.poll() is not None:
        raise _XvfbProcessError('Failed to start OpenBox.')

    if use_xcompmgr:
      xcompmgr_proc = subprocess.Popen(
          'xcompmgr', stderr=subprocess.STDOUT, env=env)

    return test_env.run_executable(cmd, env, stdoutfile)
  except OSError as e:
    print('Failed to start Xvfb or Openbox: %s\n' % str(e), file=sys.stderr)
    return 1
  except _XvfbProcessError as e:
    print('Xvfb fail: %s\n' % str(e), file=sys.stderr)
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

    dbus_pid = launch_dbus(env)

    # The bundled weston (//third_party/weston) is used by Linux Ozone Wayland
    # CI and CQ testers and compiled by //ui/ozone/platform/wayland whenever
    # there is a dependency on the Ozone/Wayland and use_bundled_weston is set
    # in gn args. However, some tests do not require Wayland or do not use
    # //ui/ozone at all, but still have --use-weston flag set by the
    # OZONE_WAYLAND variant (see //testing/buildbot/variants.pyl). This results
    # in failures and those tests cannot be run because of the exception that
    # informs about missing weston binary. Thus, to overcome the issue before
    # a better solution is found, add a check for the "weston" binary here and
    # run tests without Wayland compositor if the weston binary is not found.
    # TODO(https://1178788): find a better solution.
    if not os.path.isfile("./weston"):
      print('Weston is not available. Starting without Wayland compositor')
      return test_env.run_executable(cmd, env, stdoutfile)

    # Set $XDG_RUNTIME_DIR if it is not set.
    _set_xdg_runtime_dir(env)

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
    # 4) --use-gl - Runs Weston using hardware acceleration instead of
    # SwiftShader.
    weston_cmd = ['./weston', '--backend=headless-backend.so', '--idle-time=0',
          '--width=1024', '--height=768', '--modules=test-plugin.so']

    if '--weston-use-gl' in cmd:
      weston_cmd.append('--use-gl')
      cmd.remove('--weston-use-gl')

    if '--weston-debug-logging' in cmd:
      cmd.remove('--weston-debug-logging')
      env = copy.deepcopy(env)
      env['WAYLAND_DEBUG'] = '1'

    weston_proc_display = None
    for _ in range(10):
      weston_proc = subprocess.Popen(
         weston_cmd,
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
    print('Failed to start Weston: %s\n' % str(e), file=sys.stderr)
    return 1
  except _WestonProcessError as e:
    print('Weston fail: %s\n' % str(e), file=sys.stderr)
    return 1
  finally:
    kill(weston_proc, 'weston')

    # dbus-daemon is not a subprocess, so we can't SIGTERM+waitpid() on it.
    # To ensure it exits, use SIGKILL which should be safe since all other
    # processes that it would have been servicing have exited.
    if dbus_pid:
      os.kill(dbus_pid, signal.SIGKILL)

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

  # Try 100 times as it is not known when Weston spawn child desktop shell
  # process. The most seen so far is ~50 checks/~2.5 seconds, but startup
  # is usually almost instantaneous.
  for _ in range(100):
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
      os.makedirs(runtime_dir, 0o700)
    env['XDG_RUNTIME_DIR'] = runtime_dir


def main():
  usage = 'Usage: xvfb.py [command [--no-xvfb or --use-weston] args...]'
  if len(sys.argv) < 2:
    print(usage + '\n', file=sys.stderr)
    return 2

  # If the user still thinks the first argument is the execution directory then
  # print a friendly error message and quit.
  if os.path.isdir(sys.argv[1]):
    print('Invalid command: \"%s\" is a directory\n' % sys.argv[1],
          file=sys.stderr)
    print(usage + '\n', file=sys.stderr)
    return 3

  return run_executable(sys.argv[1:], os.environ.copy())


if __name__ == '__main__':
  sys.exit(main())
