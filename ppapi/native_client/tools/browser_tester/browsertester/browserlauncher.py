#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os.path
import re
import shutil
import sys
import tempfile
import time
import urlparse

import browserprocess

class LaunchFailure(Exception):
  pass


def GetPlatform():
  if sys.platform == 'darwin':
    platform = 'mac'
  elif sys.platform.startswith('linux'):
    platform = 'linux'
  elif sys.platform in ('cygwin', 'win32'):
    platform = 'windows'
  else:
    raise LaunchFailure('Unknown platform: %s' % sys.platform)
  return platform


PLATFORM = GetPlatform()


def SelectRunCommand():
  # The subprocess module added support for .kill in Python 2.6
  assert (sys.version_info[0] >= 3 or (sys.version_info[0] == 2 and
                                       sys.version_info[1] >= 6))
  if PLATFORM == 'linux':
    return browserprocess.RunCommandInProcessGroup
  else:
    return browserprocess.RunCommandWithSubprocess


RunCommand = SelectRunCommand()

def RemoveDirectory(path):
  retry = 5
  sleep_time = 0.25
  while True:
    try:
      shutil.rmtree(path)
    except Exception:
      # Windows processes sometime hang onto files too long
      if retry > 0:
        retry -= 1
        time.sleep(sleep_time)
        sleep_time *= 2
      else:
        # No luck - don't mask the error
        raise
    else:
      # succeeded
      break



# In Windows, subprocess seems to have an issue with file names that
# contain spaces.
def EscapeSpaces(path):
  if PLATFORM == 'windows' and ' ' in path:
    return '"%s"' % path
  return path


def MakeEnv(options):
  env = dict(os.environ)
  # Enable PPAPI Dev interfaces for testing.
  env['NACL_ENABLE_PPAPI_DEV'] = str(options.enable_ppapi_dev)
  if options.debug:
    env['NACL_PLUGIN_DEBUG'] = '1'
    # env['NACL_SRPC_DEBUG'] = '1'
  return env


class BrowserLauncher(object):

  WAIT_TIME = 20
  WAIT_STEPS = 80
  SLEEP_TIME = float(WAIT_TIME) / WAIT_STEPS

  def __init__(self, options):
    self.options = options
    self.profile = None
    self.binary = None
    self.tool_log_dir = None

  def KnownPath(self):
    raise NotImplementedError

  def BinaryName(self):
    raise NotImplementedError

  def CreateProfile(self):
    raise NotImplementedError

  def MakeCmd(self, url, host, port):
    raise NotImplementedError

  def CreateToolLogDir(self):
    self.tool_log_dir = tempfile.mkdtemp(prefix='vglogs_')
    return self.tool_log_dir

  def FindBinary(self):
    if self.options.browser_path:
      return self.options.browser_path
    else:
      path = self.KnownPath()
      if path is None or not os.path.exists(path):
        raise LaunchFailure('Cannot find the browser directory')
      binary = os.path.join(path, self.BinaryName())
      if not os.path.exists(binary):
        raise LaunchFailure('Cannot find the browser binary')
      return binary

  def WaitForProcessDeath(self):
    self.browser_process.Wait(self.WAIT_STEPS, self.SLEEP_TIME)

  def Cleanup(self):
    self.browser_process.Kill()

    RemoveDirectory(self.profile)
    if self.tool_log_dir is not None:
      RemoveDirectory(self.tool_log_dir)

  def MakeProfileDirectory(self):
    self.profile = tempfile.mkdtemp(prefix='browserprofile_')
    return self.profile

  def SetStandardStream(self, env, var_name, redirect_file, is_output):
    if redirect_file is None:
      return
    file_prefix = 'file:'
    dev_prefix = 'dev:'
    debug_warning = 'DEBUG_ONLY:'
    # logic must match src/trusted/service_runtime/nacl_resource.*
    # resource specification notation.  file: is the default
    # interpretation, so we must have an exhaustive list of
    # alternative schemes accepted.  if we remove the file-is-default
    # interpretation, replace with
    #   is_file = redirect_file.startswith(file_prefix)
    # and remove the list of non-file schemes.
    is_file = (not (redirect_file.startswith(dev_prefix) or
                    redirect_file.startswith(debug_warning + dev_prefix)))
    if is_file:
      if redirect_file.startswith(file_prefix):
        bare_file = redirect_file[len(file_prefix)]
      else:
        bare_file = redirect_file
      # why always abspath?  does chrome chdir or might it in the
      # future?  this means we do not test/use the relative path case.
      redirect_file = file_prefix + os.path.abspath(bare_file)
    else:
      bare_file = None  # ensure error if used without checking is_file
    env[var_name] = redirect_file
    if is_output:
      # sel_ldr appends program output to the file so we need to clear it
      # in order to get the stable result.
      if is_file:
        if os.path.exists(bare_file):
          os.remove(bare_file)
        parent_dir = os.path.dirname(bare_file)
        # parent directory may not exist.
        if not os.path.exists(parent_dir):
          os.makedirs(parent_dir)

  def Launch(self, cmd, env):
    browser_path = cmd[0]
    if not os.path.exists(browser_path):
      raise LaunchFailure('Browser does not exist %r'% browser_path)
    if not os.access(browser_path, os.X_OK):
      raise LaunchFailure('Browser cannot be executed %r (Is this binary on an '
                          'NFS volume?)' % browser_path)
    if self.options.sel_ldr:
      env['NACL_SEL_LDR'] = self.options.sel_ldr
    if self.options.sel_ldr_bootstrap:
      env['NACL_SEL_LDR_BOOTSTRAP'] = self.options.sel_ldr_bootstrap
    if self.options.irt_library:
      env['NACL_IRT_LIBRARY'] = self.options.irt_library
    self.SetStandardStream(env, 'NACL_EXE_STDIN',
                           self.options.nacl_exe_stdin, False)
    self.SetStandardStream(env, 'NACL_EXE_STDOUT',
                           self.options.nacl_exe_stdout, True)
    self.SetStandardStream(env, 'NACL_EXE_STDERR',
                           self.options.nacl_exe_stderr, True)
    print('ENV:', ' '.join(['='.join(pair) for pair in env.items()]))
    print('LAUNCHING: %s' % ' '.join(cmd))
    sys.stdout.flush()
    self.browser_process = RunCommand(cmd, env=env)

  def IsRunning(self):
    return self.browser_process.IsRunning()

  def GetReturnCode(self):
    return self.browser_process.GetReturnCode()

  def Run(self, url, host, port):
    self.binary = EscapeSpaces(self.FindBinary())
    self.profile = self.CreateProfile()
    if self.options.tool is not None:
      self.tool_log_dir = self.CreateToolLogDir()
    cmd = self.MakeCmd(url, host, port)
    self.Launch(cmd, MakeEnv(self.options))


def EnsureDirectory(path):
  if not os.path.exists(path):
    os.makedirs(path)


def EnsureDirectoryForFile(path):
  EnsureDirectory(os.path.dirname(path))


class ChromeLauncher(BrowserLauncher):

  def KnownPath(self):
    if PLATFORM == 'linux':
      # TODO(ncbray): look in path?
      return '/opt/google/chrome'
    elif PLATFORM == 'mac':
      return '/Applications/Google Chrome.app/Contents/MacOS'
    else:
      homedir = os.path.expanduser('~')
      path = os.path.join(homedir, r'AppData\Local\Google\Chrome\Application')
      return path

  def BinaryName(self):
    if PLATFORM == 'mac':
      return 'Google Chrome'
    elif PLATFORM == 'windows':
      return 'chrome.exe'
    else:
      return 'chrome'

  def MakeEmptyJSONFile(self, path):
    EnsureDirectoryForFile(path)
    f = open(path, 'w')
    f.write('{}')
    f.close()

  def CreateProfile(self):
    profile = self.MakeProfileDirectory()

    # Squelch warnings by creating bogus files.
    self.MakeEmptyJSONFile(os.path.join(profile, 'Default', 'Preferences'))
    self.MakeEmptyJSONFile(os.path.join(profile, 'Local State'))

    return profile

  def NetLogName(self):
    return os.path.join(self.profile, 'netlog.json')

  def MakeCmd(self, url, host, port):
    cmd = [self.binary,
            # --enable-logging enables stderr output from Chromium subprocesses
            # on Windows (see
            # https://code.google.com/p/chromium/issues/detail?id=171836)
            '--enable-logging',
            # This prevents Chrome from making "hidden" network requests at
            # startup and navigation.  These requests could be a source of
            # non-determinism, and they also add noise to the netlogs.
            '--disable-features=NetworkPrediction',
            # This is speculative, sync should not occur with a clean profile.
            '--disable-sync',
            '--no-first-run',
            '--no-default-browser-check',
            '--log-level=1',
            '--disable-default-apps',
            # Suppress metrics reporting.  This prevents misconfigured bots,
            # people testing at their desktop, etc from poisoning the UMA data.
            '--metrics-recording-only',
            # Chrome explicitly blacklists some ports as "unsafe" because
            # certain protocols use them.  Chrome gives an error like this:
            # Error 312 (net::ERR_UNSAFE_PORT): Unknown error
            # Unfortunately, the browser tester can randomly choose a
            # blacklisted port.  To work around this, the tester whitelists
            # whatever port it is using.
            '--explicitly-allowed-ports=%d' % port,
            '--user-data-dir=%s' % self.profile]
    # Log network requests to assist debugging.
    cmd.append('--log-net-log=%s' % self.NetLogName())
    if PLATFORM == 'linux':
      # Explicitly run with SwiftShader on linux. The test infrastructure
      # doesn't have sufficient native GL contextes to run these tests.
      cmd.append('--use-gl=swiftshader')
    if self.options.ppapi_plugin is None:
      cmd.append('--enable-nacl')
      disable_sandbox = False
      # Chrome process can't access file within sandbox
      disable_sandbox |= self.options.nacl_exe_stdin is not None
      disable_sandbox |= self.options.nacl_exe_stdout is not None
      disable_sandbox |= self.options.nacl_exe_stderr is not None
      if disable_sandbox:
        cmd.append('--no-sandbox')
    else:
      cmd.append('--register-pepper-plugins=%s;%s'
                 % (self.options.ppapi_plugin,
                    self.options.ppapi_plugin_mimetype))
      cmd.append('--no-sandbox')
    if self.options.browser_extensions:
      cmd.append('--load-extension=%s' %
                 ','.join(self.options.browser_extensions))
      cmd.append('--enable-experimental-extension-apis')
    if self.options.enable_crash_reporter:
      cmd.append('--enable-crash-reporter-for-testing')
    if self.options.tool == 'memcheck':
      cmd = ['src/third_party/valgrind/memcheck.sh',
             '-v',
             '--xml=yes',
             '--leak-check=no',
             '--gen-suppressions=all',
             '--num-callers=30',
             '--trace-children=yes',
             '--nacl-file=%s' % (self.options.files[0],),
             '--suppressions=' +
             '../tools/valgrind/memcheck/suppressions.txt',
             '--xml-file=%s/xml.%%p' % (self.tool_log_dir,),
             '--log-file=%s/log.%%p' % (self.tool_log_dir,)] + cmd
    elif self.options.tool == 'tsan':
      cmd = ['src/third_party/valgrind/tsan.sh',
             '-v',
             '--num-callers=30',
             '--trace-children=yes',
             '--nacl-file=%s' % (self.options.files[0],),
             '--ignore=../tools/valgrind/tsan/ignores.txt',
             '--suppressions=../tools/valgrind/tsan/suppressions.txt',
             '--log-file=%s/log.%%p' % (self.tool_log_dir,)] + cmd
    elif self.options.tool != None:
      raise LaunchFailure('Invalid tool name "%s"' % (self.options.tool,))
    if self.options.enable_sockets:
      cmd.append('--allow-nacl-socket-api=%s' % host)
    cmd.extend(self.options.browser_flags)
    cmd.append(url)
    return cmd
