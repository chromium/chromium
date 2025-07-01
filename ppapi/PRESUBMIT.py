# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import os
import re
import subprocess
import sys


# In this file `sys.executable` is used instead of
# `input_api.python3_executable` because on Windows
# `input_api.python3_executable` is `vpython3.bat` whereas `sys.executable` is
# `python.exe`. If `input_api.python3_executable` is used, we need to explicitly
# pass `shell=True` to `subprocess.Popen()`, which is a security risk
# (https://docs.python.org/3/library/subprocess.html#security-considerations).
#
# TODO: Investigate the incompatibility of `input_api.python3_executable` on
# Windows, for this particular PRESUBMIT script.

def RunCmdAndCheck(cmd, err_string, output_api, cwd=None, warning=False):
  results = []
  p = subprocess.Popen(cmd, cwd=cwd,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
  (_, p_stderr) = p.communicate()
  if p.returncode:
    if warning:
      results.append(output_api.PresubmitPromptWarning(
        '%s\n\n%s' % (err_string, p_stderr.decode('utf-8'))))
    else:
      results.append(
          output_api.PresubmitError(err_string,
                                    long_text=p_stderr.decode('utf-8')))
  return results


def RunUnittests(input_api, output_api):
  # Run some Generator unittests if the generator source was changed.
  results = []
  files = input_api.LocalPaths()
  generator_files = []
  for filename in files:
    name_parts = filename.split(os.sep)
    if name_parts[0:2] == ['ppapi', 'generators']:
      generator_files.append(filename)
  if generator_files != []:
    cmd = [sys.executable, 'idl_tests.py']
    ppapi_dir = input_api.PresubmitLocalPath()
    results.extend(RunCmdAndCheck(cmd,
                                  'PPAPI IDL unittests failed.',
                                  output_api,
                                  os.path.join(ppapi_dir, 'generators')))
  return results


# Verify that the files do not contain a 'TODO' in them.
RE_TODO = re.compile(r'\WTODO\W', flags=re.I)
def CheckTODO(input_api, output_api):
  live_files = input_api.AffectedFiles(include_deletes=False)
  files = [f.LocalPath() for f in live_files]
  todo = []

  for filename in files:
    name, ext = os.path.splitext(filename)
    name_parts = name.split(os.sep)

    # Only check normal build sources.
    if ext not in ['.h', '.idl']:
      continue

    # Only examine the ppapi directory.
    if name_parts[0] != 'ppapi':
      continue

    # Only examine public plugin facing directories.
    if name_parts[1] not in ['api', 'c', 'cpp', 'utility']:
      continue

    # Only examine public stable interfaces.
    if name_parts[2] in ['dev', 'private', 'trusted']:
      continue

    filepath = os.path.join('..', filename)
    with io.open(filepath, encoding='utf-8') as f:
      if RE_TODO.search(f.read()):
        todo.append(filename)

  if todo:
    return [output_api.PresubmitPromptWarning(
        'TODOs found in stable public PPAPI files:',
        long_text='\n'.join(todo))]
  return []

# Verify that no CPP wrappers use un-versioned PPB interface name macros.
RE_UNVERSIONED_PPB = re.compile(r'\bPPB_\w+_INTERFACE\b')
def CheckUnversionedPPB(input_api, output_api):
  live_files = input_api.AffectedFiles(include_deletes=False)
  files = [f.LocalPath() for f in live_files]
  todo = []

  for filename in files:
    name, ext = os.path.splitext(filename)
    name_parts = name.split(os.sep)

    # Only check C++ sources.
    if ext not in ['.cc']:
      continue

    # Only examine the public plugin facing ppapi/cpp directory.
    if name_parts[0:2] != ['ppapi', 'cpp']:
      continue

    # Only examine public stable and trusted interfaces.
    if name_parts[2] in ['dev', 'private']:
      continue

    filepath = os.path.join('..', filename)
    with io.open(filepath, encoding='utf-8') as f:
      if RE_UNVERSIONED_PPB.search(f.read()):
        todo.append(filename)

  if todo:
    return [output_api.PresubmitError(
        'Unversioned PPB interface references found in PPAPI C++ wrappers:',
        long_text='\n'.join(todo))]
  return []

# Verify that changes to ppapi headers/sources are also made to NaCl SDK.
def CheckUpdatedNaClSDK(input_api, output_api):
  files = input_api.LocalPaths()

  # PPAPI files the Native Client SDK cares about.
  nacl_sdk_files = []

  for filename in files:
    name, ext = os.path.splitext(filename)
    name_parts = name.split(os.sep)

    if len(name_parts) <= 2:
      continue

    if name_parts[0] != 'ppapi':
      continue

    if ((name_parts[1] == 'c' and ext == '.h') or
        (name_parts[1] in ('cpp', 'utility') and ext in ('.h', '.cc'))):
      if name_parts[2] in ('documentation', 'trusted'):
        continue
      nacl_sdk_files.append(filename)

  if not nacl_sdk_files:
    return []

  verify_ppapi_py = os.path.join(input_api.change.RepositoryRoot(),
                                 'native_client_sdk', 'src', 'build_tools',
                                 'verify_ppapi.py')
  # When running git cl presubmit --all this presubmit may be asked to check
  # ~300 files, leading to a command line that is ~9,500 characters, which
  # exceeds the Windows 8191 character cmd.exe limit and causes cryptic failures
  # with no context. To avoid these we break the command up into smaller pieces.
  # The error is:
  #     The command line is too long.
  files_per_command = 25 if input_api.is_windows else 1000
  results = []
  for i in range(0, len(nacl_sdk_files), files_per_command):
    cmd = [sys.executable, verify_ppapi_py
           ] + nacl_sdk_files[i:i + files_per_command]
    results.extend(
        RunCmdAndCheck(
            cmd,'PPAPI Interface modified without updating NaCl SDK.\n'
                '(note that some dev interfaces should not be added '
                'the NaCl SDK; when in doubt, ask a ppapi OWNER.\n'
                'To ignore a file, add it to IGNORED_FILES in '
                'native_client_sdk/src/build_tools/verify_ppapi.py)',
                output_api,
                warning=True))
  return results

# Verify that changes to ppapi/thunk/interfaces_* files have a corresponding
# change to tools/metrics/histograms/enums.xml for UMA tracking.
def CheckHistogramXml(input_api, output_api):
  # We can't use input_api.LocalPaths() here because we need to know about
  # changes outside of ppapi/. See tools/depot_tools/presubmit_support.py for
  # details on input_api.
  files = input_api.change.AffectedFiles()

  INTERFACE_FILES = ('ppapi/thunk/interfaces_legacy.h',
                     'ppapi/thunk/interfaces_ppb_private.h',
                     'ppapi/thunk/interfaces_ppb_private_no_permissions.h',
                     'ppapi/thunk/interfaces_ppb_public_dev_channel.h',
                     'ppapi/thunk/interfaces_ppb_public_dev.h',
                     'ppapi/thunk/interfaces_ppb_public_stable.h',
                     'ppapi/thunk/interfaces_ppb_public_socket.h')
  HISTOGRAM_XML_FILE = 'tools/metrics/histograms/enums.xml'
  interface_changes = []
  has_histogram_xml_change = False
  for filename in files:
    path = filename.LocalPath()
    if path in INTERFACE_FILES:
      interface_changes.append(path)
    if path == HISTOGRAM_XML_FILE:
      has_histogram_xml_change = True

  if interface_changes and not has_histogram_xml_change:
    return [output_api.PresubmitNotifyResult(
        'Missing change to tools/metrics/histograms/enums.xml.\n' +
        'Run pepper_hash_for_uma to make get values for new interfaces.\n' +
        'Interface changes:\n' + '\n'.join(interface_changes))]
  return []

def CheckChange(input_api, output_api):
  results = []

  results.extend(RunUnittests(input_api, output_api))

  results.extend(CheckTODO(input_api, output_api))

  results.extend(CheckUnversionedPPB(input_api, output_api))

  results.extend(CheckUpdatedNaClSDK(input_api, output_api))

  results.extend(CheckHistogramXml(input_api, output_api))

  # Verify all modified *.idl have a matching *.h
  files = input_api.LocalPaths()
  h_files = []
  idl_files = []
  generators_changed = False

  # These are autogenerated by the command buffer generator, they don't go
  # through idl.
  whitelist = ['ppb_opengles2', 'ppb_opengles2ext_dev']

  # Find all relevant .h and .idl files.
  for filename in files:
    name, ext = os.path.splitext(filename)
    name_parts = name.split(os.sep)
    if name_parts[-1] in whitelist:
      continue
    if name_parts[0:2] == ['ppapi', 'c'] and ext == '.h':
      h_files.append('/'.join(name_parts[2:]))
    elif name_parts[0:2] == ['ppapi', 'api'] and ext == '.idl':
      idl_files.append('/'.join(name_parts[2:]))
    elif name_parts[0:2] == ['ppapi', 'generators']:
      generators_changed = True

  # Generate a list of all appropriate *.h and *.idl changes in this CL.
  both = h_files + idl_files

  # If there aren't any, we are done checking.
  if not both: return results

  missing = []
  for filename in idl_files:
    if filename not in set(h_files):
      missing.append('ppapi/api/%s.idl' % filename)

  # An IDL change that includes [generate_thunk] doesn't need to have
  # an update to the corresponding .h file.
  new_thunk_files = []
  for filename in missing:
    lines = input_api.RightHandSideLines(lambda f: f.LocalPath() == filename)
    for line in lines:
      if line[2].strip() == '[generate_thunk]':
        new_thunk_files.append(filename)
  for filename in new_thunk_files:
    missing.remove(filename)

  if missing:
    results.append(
        output_api.PresubmitPromptWarning(
            'Missing PPAPI header, no change or skipped generation?',
            long_text='\n  '.join(missing)))

  missing_dev = []
  missing_stable = []
  missing_priv = []
  for filename in h_files:
    if filename not in set(idl_files):
      name_parts = filename.split(os.sep)

      if name_parts[-1] == 'pp_macros':
        # The C header generator adds a PPAPI_RELEASE macro based on all the
        # IDL files, so pp_macros.h may change while its IDL does not.
        lines = input_api.RightHandSideLines(
            lambda f: f.LocalPath() == 'ppapi/c/%s.h' % filename)
        releaseChanged = False
        for line in lines:
          if line[2].split()[:2] == ['#define', 'PPAPI_RELEASE']:
            results.append(
                output_api.PresubmitPromptOrNotify(
                    'PPAPI_RELEASE has changed', long_text=line[2]))
            releaseChanged = True
            break
        if releaseChanged:
          continue

      if 'trusted' in name_parts:
        missing_priv.append('  ppapi/c/%s.h' % filename)
        continue

      if 'private' in name_parts:
        missing_priv.append('  ppapi/c/%s.h' % filename)
        continue

      if 'dev' in name_parts:
        missing_dev.append('  ppapi/c/%s.h' % filename)
        continue

      missing_stable.append('  ppapi/c/%s.h' % filename)

  if missing_priv:
    results.append(
        output_api.PresubmitPromptWarning(
            'Missing PPAPI IDL for private interface, please generate IDL:',
            long_text='\n'.join(missing_priv)))

  if missing_dev:
    results.append(
        output_api.PresubmitPromptWarning(
            'Missing PPAPI IDL for DEV, required before moving to stable:',
            long_text='\n'.join(missing_dev)))

  if missing_stable:
    # It might be okay that the header changed without a corresponding IDL
    # change. E.g., comment indenting may have been changed. Treat this as a
    # warning.
    if generators_changed:
      results.append(
          output_api.PresubmitPromptWarning(
              'Missing PPAPI IDL for stable interface (due to change in ' +
              'generators?):',
              long_text='\n'.join(missing_stable)))
    else:
      results.append(
          output_api.PresubmitError(
              'Missing PPAPI IDL for stable interface:',
              long_text='\n'.join(missing_stable)))

  # Verify all *.h files match *.idl definitions, use:
  #   --test to prevent output to disk
  #   --diff to generate a unified diff
  #   --out to pick which files to examine (only the ones in the CL)
  ppapi_dir = input_api.PresubmitLocalPath()
  cmd = [sys.executable, 'generator.py',
         '--wnone', '--diff', '--test','--cgen', '--range=start,end']

  # Only generate output for IDL files references (as *.h or *.idl) in this CL
  cmd.append('--out=' + ','.join([name + '.idl' for name in both]))
  cmd_results = RunCmdAndCheck(cmd,
                               'PPAPI IDL Diff detected: Run the generator.',
                               output_api,
                               os.path.join(ppapi_dir, 'generators'))
  if cmd_results:
    results.extend(cmd_results)

  return results


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
