# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Audit that the logging shim's export routing agrees with the allowlist.

basic_sandbox.def is the in-repo basic-process API allowlist: the set of
Windows APIs that are expected to be reachable inside the basic process.
shim/exports_logging.def routes each exported name to one of:

  * a direct pass-through forward (``Name = kernel32.Name``) — the call is
    silently forwarded to the real API and NOT logged;
  * a logging / modified-behavior thunk (``Name = ApifwName``) — the call is
    logged (and possibly normalized) before forwarding;
  * the shared crash-sink (``Name = ApifwNotReached``) — the API has never
    been observed and NOTREACHED()s if it is ever called.

Two invariants must hold, or the two files have silently drifted:

  1. Every DIRECT-FORWARD name must be on the allowlist. A direct forward is an
     unlogged pass-through to the real API, so forwarding a non-allowlisted API
     would be a silent hole: the whole point of the shim is that only
     allowlisted APIs are reachable without observation.

  2. No allowlisted name may be routed to ApifwNotReached. The allowlist says
     the API is expected to be reachable, so crash-sinking it contradicts the
     allowlist. (Allowlisted names routed to a logging/modified thunk are fine:
     loader APIs such as GetProcAddress/LoadLibraryW are allowlisted yet logged
     on purpose so their arguments can be observed.)

This runs as a build action so the invariants are enforced at build time; it
exits non-zero (failing the build) and lists the offending names on drift.

Usage:
  python3 audit_allowlist_coverage.py --allowlist <basic_sandbox.def> \
      --logging-def <exports_logging.def> [--stamp <path>]
"""

import argparse
import sys

from def_parser import parse_def_exports


def parse_allowlist(path):
  """Return the set of API names from an allowlist .def file."""
  names = set()
  with open(path, 'r', encoding='utf-8') as f:
    for line in f:
      stripped = line.strip()
      if (not stripped or stripped.startswith(';') or
          stripped.upper() == 'EXPORTS'):
        continue
      names.add(stripped.split()[0])
  return names


def parse_export_routing(path):
  """Return (direct_forwards, notreached) name sets from a logging .def file.

  direct_forwards: names aliased to a real system export (``Name = kern.Name``).
  notreached:      names aliased to the ApifwNotReached crash-sink.
  Names aliased to any other Apifw* thunk are neither (logged/modified).
  """
  direct_forwards = set()
  notreached = set()
  for export in parse_def_exports(path):
    if export.target == 'ApifwNotReached':
      notreached.add(export.name)
    elif (export.target and '.' in export.target and
          not export.target.startswith('Apifw')):
      # "Name = module.Export" — a PE forward straight to the real API.
      direct_forwards.add(export.name)
  return direct_forwards, notreached


def main():
  parser = argparse.ArgumentParser(
      description='Audit allowlist coverage of the logging shim export table')
  parser.add_argument('--allowlist', required=True,
                      help='Path to basic_sandbox.def')
  parser.add_argument('--logging-def', required=True,
                      help='Path to shim/exports_logging.def')
  parser.add_argument('--stamp', help='Stamp file written on success')
  args = parser.parse_args()

  allowlist = parse_allowlist(args.allowlist)
  direct_forwards, notreached = parse_export_routing(args.logging_def)

  # Invariant 1: an unlogged direct forward must be allowlisted.
  forwarded_not_allowlisted = sorted(direct_forwards - allowlist)
  # Invariant 2: an allowlisted (expected) API must not be crash-sunk.
  allowlisted_notreached = sorted(allowlist & notreached)

  if forwarded_not_allowlisted or allowlisted_notreached:
    sys.stderr.write(
        'basic-process allowlist / logging shim drift '
        '(basic_sandbox.def vs exports_logging.def):\n')
    if forwarded_not_allowlisted:
      sys.stderr.write(
          '  direct-forwarded (silently, unlogged) but NOT on the allowlist —\n'
          '  add to basic_sandbox.def or route to a logging thunk:\n')
      for name in forwarded_not_allowlisted:
        sys.stderr.write(f'    {name}\n')
    if allowlisted_notreached:
      sys.stderr.write(
          '  on the allowlist but routed to ApifwNotReached — an expected\n'
          '  API must not crash-sink; route it to a forward or logging '
          'thunk:\n')
      for name in allowlisted_notreached:
        sys.stderr.write(f'    {name}\n')
    return 1

  if args.stamp:
    with open(args.stamp, 'w', encoding='utf-8') as f:
      f.write('ok\n')
  return 0


if __name__ == '__main__':
  sys.exit(main())
