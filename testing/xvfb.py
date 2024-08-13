#!/usr/bin/env vpython3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs tests with Xvfb or Xorg and Openbox or Weston on Linux and normally on
other platforms."""

from __future__ import print_function

import copy
import os
import os.path
import random
import re
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
import uuid

import psutil

import test_env

DEFAULT_XVFB_WHD = '1280x800x24'

# pylint: disable=useless-object-inheritance

class _X11ProcessError(Exception):
  """Exception raised when Xvfb or Xorg cannot start."""


class _WestonProcessError(Exception):
  """Exception raised when Weston cannot start."""


def kill(proc, name, timeout_in_seconds=10):
  """Tries to kill |proc| gracefully with a timeout for each signal."""
  if not proc:
    return

  thread = threading.Thread(target=proc.wait)
  try:
    proc.terminate()
    thread.start()

    thread.join(timeout_in_seconds)
    if thread.is_alive():
      print('%s running after SIGTERM, trying SIGKILL.\n' % name,
            file=sys.stderr)
      proc.kill()
  except OSError as e:
    # proc.terminate()/kill() can raise, not sure if only ProcessLookupError
    # which is explained in https://bugs.python.org/issue40550#msg382427
    print('Exception while killing process %s: %s' % (name, e), file=sys.stderr)

  thread.join(timeout_in_seconds)
  if thread.is_alive():
    print('%s running after SIGTERM and SIGKILL; good luck!\n' % name,
          file=sys.stderr)


def launch_dbus(env):  # pylint: disable=inconsistent-return-statements
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
    dbus_output = subprocess.check_output(['dbus-launch'],
                                          env=env).decode('utf-8').split('\n')
    for line in dbus_output:
      m = re.match(r'([^=]+)\=(.+)', line)
      if m:
        env[m.group(1)] = m.group(2)
    return int(env['DBUS_SESSION_BUS_PID'])
  except (subprocess.CalledProcessError, OSError, KeyError, ValueError) as e:
    print('Exception while running dbus_launch: %s' % e)


# TODO(crbug.com/40621504): Encourage setting flags to False.
def run_executable(cmd,
                   env,
                   stdoutfile=None,
                   use_openbox=True,
                   use_xcompmgr=True,
                   xvfb_whd=None,
                   cwd=None):
  """Runs an executable within Weston, Xvfb or Xorg on Linux or normally on
     other platforms.

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
    xvfb_whd: WxHxD to pass to xvfb or DEFAULT_XVFB_WHD if None
    cwd: Current working directory.

  Returns:
    the exit code of the specified commandline, or 1 on failure.
  """

  # It might seem counterintuitive to support a --no-xvfb flag in a script
  # whose only job is to start xvfb, but doing so allows us to consolidate
  # the logic in the layers of buildbot scripts so that we *always* use
  # this script by default and don't have to worry about the distinction, it
  # can remain solely under the control of the test invocation itself.
  # Historically, this flag turned off xvfb, but now turns off both X11 backings
  # (xvfb/Xorg). As of crrev.com/c/5631242, Xorg became the default backing when
  # no flags are supplied. Xorg is mostly a drop in replacement to Xvfb but has
  # better support for dummy drivers and multi-screen testing (See:
  # crbug.com/40257169 and http://tinyurl.com/4phsuupf). Requires Xorg binaries
  # (package: xserver-xorg-core)
  use_xvfb = False
  use_xorg = True

  if '--no-xvfb' in cmd:
    use_xvfb = False
    use_xorg = False  # Backwards compatibly turns off all X11 backings.
    cmd.remove('--no-xvfb')

  # Support forcing legacy xvfb backing.
  if '--use-xvfb' in cmd:
    if not use_xorg and not use_xvfb:
      print('Conflicting flags --use-xvfb and --no-xvfb\n', file=sys.stderr)
      return 1
    use_xvfb = True
    use_xorg = False
    cmd.remove('--use-xvfb')

  # Tests that run on Linux platforms with Ozone/Wayland backend require
  # a Weston instance. However, it is also required to disable xvfb so
  # that Weston can run in a pure headless environment.
  use_weston = False
  if '--use-weston' in cmd:
    if use_xvfb or use_xorg:
      print('Unable to use Weston with xvfb or Xorg.\n', file=sys.stderr)
      return 1
    use_weston = True
    cmd.remove('--use-weston')

  if sys.platform.startswith('linux') and (use_xvfb or use_xorg):
    return _run_with_x11(cmd, env, stdoutfile, use_openbox, use_xcompmgr,
                         use_xorg, xvfb_whd or DEFAULT_XVFB_WHD, cwd)
  if use_weston:
    return _run_with_weston(cmd, env, stdoutfile, cwd)
  return test_env.run_executable(cmd, env, stdoutfile, cwd)


def _re_search_command(regex, args, **kwargs):
  """Runs a subprocess defined by `args` and returns a regex match for the
  given expression on the output."""
  return re.search(
      regex,
      subprocess.check_output(args,
                              stderr=subprocess.STDOUT,
                              text=True,
                              **kwargs), re.IGNORECASE)


def _make_xorg_modeline(width, height, refresh):
  """Generates a tuple of a modeline (list of parameters) and label based off a
  specified width, height and refresh rate.
  See: https://www.x.org/archive/X11R7.0/doc/html/chips4.html"""
  re_matches = _re_search_command(
      r'Modeline "(.*)"\s+(.*)',
      ['cvt', str(width), str(height),
       str(refresh)],
  )
  modeline_label = re_matches.group(1)
  modeline = re_matches.group(2)
  # Split the modeline string on spaces, and filter out empty element (cvt adds
  # double spaces between in some parts).
  return (modeline_label, list(filter(lambda a: a != '', modeline.split(' '))))


def _get_supported_virtual_sizes(default_whd):
  """Returns a list of tuples (width, height) for supported monitor resolutions.
  The list will always include the default size defined in `default_whd`"""
  # Note: 4K resolution 3840x2160 doesn't seem to be supported and the mode
  # silently gets dropped which makes subsequent calls to xrandr --addmode fail.
  (default_width, default_height, _) = default_whd.split('x')
  default_size = (int(default_width), int(default_height))
  return sorted(
      set([default_size, (800, 600), (1024, 768), (1920, 1080), (1600, 1200)]))


def _make_xorg_config(default_whd):
  """Generates an Xorg config file and returns the file path. See:
  https://www.x.org/releases/current/doc/man/man5/xorg.conf.5.xhtml"""
  (_, _, depth) = default_whd.split('x')
  mode_sizes = _get_supported_virtual_sizes(default_whd)
  modelines = []
  mode_labels = []
  for width, height in mode_sizes:
    (modeline_label, modeline) = _make_xorg_modeline(width, height, 60)
    modelines.append('Modeline "%s" %s' % (modeline_label, ' '.join(modeline)))
    mode_labels.append('"%s"' % modeline_label)
  config = """
Section "Monitor"
  Identifier "Monitor0"
  HorizSync 5.0 - 1000.0
  VertRefresh 5.0 - 200.0
  %s
EndSection
Section "Device"
  Identifier "Device0"
  # Dummy driver requires package `xserver-xorg-video-dummy`.
  Driver "dummy"
  VideoRam 256000
EndSection
Section "Screen"
  Identifier "Screen0"
  Device "Device0"
  Monitor "Monitor0"
  SubSection "Display"
    Depth %s
    Modes %s
  EndSubSection
EndSection
  """ % ('\n'.join(modelines), depth, ' '.join(mode_labels))
  config_file = os.path.join(tempfile.gettempdir(),
                             'xorg-%s.config' % uuid.uuid4().hex)
  with open(config_file, 'w') as f:
    f.write(config)
  return config_file

def _setup_xrandr(env, default_whd):
  """Configures xrandr display(s)"""

  # Calls xrandr with the provided argument array
  def call_xrandr(args):
    subprocess.check_call(['xrandr'] + args,
                          env=env,
                          stdout=subprocess.DEVNULL,
                          stderr=subprocess.STDOUT)

  (default_width, default_height, _) = default_whd.split('x')
  default_size = (int(default_width), int(default_height))

  # The minimum version of xserver-xorg-video-dummy is 0.4.0-1 which adds
  # XRANDR support. Older versions will be missing the "DUMMY" outputs.
  # Reliably checking the version is difficult, so check if the xrandr output
  # includes the DUMMY displays before trying to configure them.
  dummy_displays_available = _re_search_command('DUMMY[0-9]', ['xrandr', '-q'],
                                                env=env)
  if dummy_displays_available:
    screen_sizes = _get_supported_virtual_sizes(default_whd)
    output_names = ['DUMMY0', 'DUMMY1', 'DUMMY2', 'DUMMY3', 'DUMMY4']
    refresh_rate = 60
    for width, height in screen_sizes:
      (modeline_label, _) = _make_xorg_modeline(width, height, 60)
      for output_name in output_names:
        call_xrandr(['--addmode', output_name, modeline_label])
    (default_mode_label, _) = _make_xorg_modeline(*default_size, refresh_rate)
    # Set the mode of all monitors to connect and activate them.
    for i, name in enumerate(output_names):
      args = ['--output', name, '--mode', default_mode_label]
      if i > 0:
        args += ['--right-of', output_names[i - 1]]
      call_xrandr(args)

  # Sets the primary monitor to the default size and marks the rest as disabled.
  call_xrandr(['-s', '%dx%d' % default_size])
  # Set the DPI to something realistic (as required by some desktops).
  call_xrandr(['--dpi', '96'])


def _run_with_x11(cmd, env, stdoutfile, use_openbox, use_xcompmgr, use_xorg,
                  xvfb_whd, cwd):
  """Runs with an X11 server. Uses Xvfb by default and Xorg when use_xorg is
  True."""
  openbox_proc = None
  openbox_ready = MutableBoolean()

  def set_openbox_ready(*_):
    openbox_ready.setvalue(True)

  xcompmgr_proc = None
  x11_proc = None
  x11_ready = MutableBoolean()

  def set_x11_ready(*_):
    x11_ready.setvalue(True)

  dbus_pid = None
  x11_binary = 'Xorg' if use_xorg else 'Xvfb'
  xorg_config_file = _make_xorg_config(xvfb_whd) if use_xorg else None
  try:
    signal.signal(signal.SIGTERM, raise_x11_error)
    signal.signal(signal.SIGINT, raise_x11_error)

    # Due to race condition for display number, Xvfb/Xorg might fail to run.
    # If it does fail, try again up to 10 times, similarly to xvfb-run.
    for _ in range(10):
      x11_ready.setvalue(False)
      display = find_display()

      x11_cmd = None
      if use_xorg:
        x11_cmd = ['Xorg', display, '-noreset', '-config', xorg_config_file]
      else:
        x11_cmd = [
            'Xvfb', display, '-screen', '0', xvfb_whd, '-ac', '-nolisten',
            'tcp', '-dpi', '96', '+extension', 'RANDR', '-maxclients', '512'
        ]

      # Sets SIGUSR1 to ignore for Xvfb/Xorg to signal current process
      # when it is ready. Due to race condition, USR1 signal could be sent
      # before the process resets the signal handler, we cannot rely on
      # signal handler to change on time.
      signal.signal(signal.SIGUSR1, signal.SIG_IGN)
      x11_proc = subprocess.Popen(x11_cmd, stderr=subprocess.STDOUT, env=env)
      signal.signal(signal.SIGUSR1, set_x11_ready)
      for _ in range(30):
        time.sleep(.1)  # gives Xvfb/Xorg time to start or fail.
        if x11_ready.getvalue() or x11_proc.poll() is not None:
          break  # xvfb/xorg sent ready signal, or already failed and stopped.

      if x11_proc.poll() is None:
        if x11_ready.getvalue():
          break  # xvfb/xorg is ready
        kill(x11_proc, x11_binary)  # still not ready, give up and retry

    if x11_proc.poll() is not None:
      raise _X11ProcessError('Failed to start after 10 tries')

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
      # Retry up to 10 times due to flaky fails (crbug.com/349187865)
      for _ in range(10):
        openbox_ready.setvalue(False)
        openbox_proc = subprocess.Popen(
            ['openbox', '--sm-disable', '--startup', openbox_startup_cmd],
            stderr=subprocess.STDOUT,
            env=env)
        for _ in range(30):
          time.sleep(.1)  # gives Openbox time to start or fail.
          if openbox_ready.getvalue() or openbox_proc.poll() is not None:
            break  # openbox sent ready signal, or failed and stopped.

        if openbox_proc.poll() is None:
          if openbox_ready.getvalue():
            break  # openbox is ready
          kill(openbox_proc, 'openbox')  # still not ready, give up and retry
          print('Openbox failed to start. Retrying.', file=sys.stderr)

      if openbox_proc.poll() is not None:
        raise _X11ProcessError('Failed to start openbox after 10 tries')

    if use_xcompmgr:
      xcompmgr_proc = subprocess.Popen('xcompmgr',
                                       stderr=subprocess.STDOUT,
                                       env=env)

    if use_xorg:
      _setup_xrandr(env, xvfb_whd)

    return test_env.run_executable(cmd, env, stdoutfile, cwd)
  except OSError as e:
    print('Failed to start %s or Openbox: %s\n' % (x11_binary, str(e)),
          file=sys.stderr)
    return 1
  except _X11ProcessError as e:
    print('%s fail: %s\n' % (x11_binary, str(e)), file=sys.stderr)
    return 1
  finally:
    kill(openbox_proc, 'openbox')
    kill(xcompmgr_proc, 'xcompmgr')
    kill(x11_proc, x11_binary)
    if xorg_config_file is not None:
      os.remove(xorg_config_file)

    # dbus-daemon is not a subprocess, so we can't SIGTERM+waitpid() on it.
    # To ensure it exits, use SIGKILL which should be safe since all other
    # processes that it would have been servicing have exited.
    if dbus_pid:
      os.kill(dbus_pid, signal.SIGKILL)


# TODO(crbug.com/40122046): Write tests.
def _run_with_weston(cmd, env, stdoutfile, cwd):
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
    if not os.path.isfile('./weston'):
      print('Weston is not available. Starting without Wayland compositor')
      return test_env.run_executable(cmd, env, stdoutfile, cwd)

    # Set $XDG_RUNTIME_DIR if it is not set.
    _set_xdg_runtime_dir(env)

    # Write options that can't be passed via CLI flags to the config file.
    # 1) panel-position=none - disables the panel, which might interfere with
    # the tests by blocking mouse input.
    with open(_weston_config_file_path(), 'w') as weston_config_file:
      weston_config_file.write('[shell]\npanel-position=none')

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
    # 3) --modules=ui-controls.so,systemd-notify.so - enables support for the
    # ui-controls Wayland protocol extension and the systemd-notify protocol.
    # 4) --width && --height set size of a virtual display: we need to set
    # an adequate size so that tests can have more room for managing size
    # of windows.
    # 5) --config=... - tells Weston to use our custom config.
    weston_cmd = [
        './weston', '--backend=headless-backend.so', '--idle-time=0',
        '--modules=ui-controls.so,systemd-notify.so', '--width=1280',
        '--height=800', '--config=' + _weston_config_file_path()
    ]

    if '--weston-use-gl' in cmd:
      # Runs Weston using hardware acceleration instead of SwiftShader.
      weston_cmd.append('--use-gl')
      cmd.remove('--weston-use-gl')

    if '--weston-debug-logging' in cmd:
      cmd.remove('--weston-debug-logging')
      env = copy.deepcopy(env)
      env['WAYLAND_DEBUG'] = '1'

    # We use the systemd-notify protocol to detect whether weston has launched
    # successfully. We listen on a unix socket and set the NOTIFY_SOCKET
    # environment variable to the socket's path. If we tell it to load its
    # systemd-notify module, weston will send a 'READY=1' message to the socket
    # once it has loaded that module.
    # See the sd_notify(3) man page and weston's compositor/systemd-notify.c for
    # more details.
    with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM
                       | socket.SOCK_NONBLOCK) as notify_socket:
      notify_socket.bind(_weston_notify_socket_address())
      env['NOTIFY_SOCKET'] = _weston_notify_socket_address()

      weston_proc_display = None
      for _ in range(10):
        weston_proc = subprocess.Popen(weston_cmd,
                                       stderr=subprocess.STDOUT,
                                       env=env)

        for _ in range(25):
          time.sleep(0.1)  # Gives weston some time to start.
          try:
            if notify_socket.recv(512) == b'READY=1':
              break
          except BlockingIOError:
            continue

        for _ in range(25):
          # The 'READY=1' message is sent as soon as weston loads the
          # systemd-notify module. This happens shortly before spawning its
          # subprocesses (e.g. desktop-shell). Wait some more to ensure they
          # have been spawned.
          time.sleep(0.1)

          # Get the $WAYLAND_DISPLAY set by Weston and pass it to the test
          # launcher. Please note that this env variable is local for the
          # process. That's the reason we have to read it from Weston
          # separately.
          weston_proc_display = _get_display_from_weston(weston_proc.pid)
          if weston_proc_display is not None:
            break  # Weston could launch and we found the display.

        # Also break from the outer loop.
        if weston_proc_display is not None:
          break

    # If we couldn't find the display after 10 tries, raise an exception.
    if weston_proc_display is None:
      raise _WestonProcessError('Failed to start Weston.')

    env.pop('NOTIFY_SOCKET')

    env['WAYLAND_DISPLAY'] = weston_proc_display
    if '--chrome-wayland-debugging' in cmd:
      cmd.remove('--chrome-wayland-debugging')
      env['WAYLAND_DEBUG'] = '1'
    else:
      env['WAYLAND_DEBUG'] = '0'

    return test_env.run_executable(cmd, env, stdoutfile, cwd)
  except OSError as e:
    print('Failed to start Weston: %s\n' % str(e), file=sys.stderr)
    return 1
  except _WestonProcessError as e:
    print('Weston fail: %s\n' % str(e), file=sys.stderr)
    return 1
  finally:
    kill(weston_proc, 'weston')

    if os.path.exists(_weston_notify_socket_address()):
      os.remove(_weston_notify_socket_address())

    if os.path.exists(_weston_config_file_path()):
      os.remove(_weston_config_file_path())

    # dbus-daemon is not a subprocess, so we can't SIGTERM+waitpid() on it.
    # To ensure it exits, use SIGKILL which should be safe since all other
    # processes that it would have been servicing have exited.
    if dbus_pid:
      os.kill(dbus_pid, signal.SIGKILL)


def _weston_notify_socket_address():
  return os.path.join(tempfile.gettempdir(), '.xvfb.py-weston-notify.sock')


def _weston_config_file_path():
  return os.path.join(tempfile.gettempdir(), '.xvfb.py-weston.ini')


def _get_display_from_weston(weston_proc_pid):
  """Retrieves $WAYLAND_DISPLAY set by Weston.

  Returns the $WAYLAND_DISPLAY variable from one of weston's subprocesses.

  Weston updates this variable early in its startup in the main process, but we
  can only read the environment variables as they were when the process was
  created. Therefore we must use one of weston's subprocesses, which are all
  spawned with the new value for $WAYLAND_DISPLAY. Any of them will do, as they
  all have the same value set.

  Args:
    weston_proc_pid: The process of id of the main Weston process.

  Returns:
    the display set by Wayland, which clients can use to connect to.
  """

  # Take the parent process.
  parent = psutil.Process(weston_proc_pid)
  if parent is None:
    return None  # The process is not found. Give up.

  # Traverse through all the children processes and find one that has
  # $WAYLAND_DISPLAY set.
  children = parent.children(recursive=True)
  for process in children:
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


def raise_x11_error(*_):
  raise _X11ProcessError('Terminated')


def raise_weston_error(*_):
  raise _WestonProcessError('Terminated')


def find_display():
  """Iterates through X-lock files to find an available display number.

  The lower bound follows xvfb-run standard at 99, and the upper bound
  is set to 119.

  Returns:
    A string of a random available display number for Xvfb ':{99-119}'.

  Raises:
    _X11ProcessError: Raised when displays 99 through 119 are unavailable.
  """

  available_displays = [
      d for d in range(99, 120)
      if not os.path.isfile('/tmp/.X{}-lock'.format(d))
  ]
  if available_displays:
    return ':{}'.format(random.choice(available_displays))
  raise _X11ProcessError('Failed to find display number')


def _set_xdg_runtime_dir(env):
  """Sets the $XDG_RUNTIME_DIR variable if it hasn't been set before."""
  runtime_dir = env.get('XDG_RUNTIME_DIR')
  if not runtime_dir:
    runtime_dir = '/tmp/xdg-tmp-dir/'
    if not os.path.exists(runtime_dir):
      os.makedirs(runtime_dir, 0o700)
    env['XDG_RUNTIME_DIR'] = runtime_dir


def main():
  usage = ('[command [--no-xvfb or --use-xvfb or --use-weston] args...]\n'
           '\t --no-xvfb\t\tTurns off all X11 backings (Xvfb and Xorg).\n'
           '\t --use-xvfb\t\tForces legacy Xvfb backing instead of Xorg.\n'
           '\t --use-weston\t\tEnable Wayland server.')
  # TODO(crbug.com/326283384): Argparse-ify this.
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
