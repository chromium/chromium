#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper script for interactive LLDB debugging via tmux."""

import argparse
import os
import re
import subprocess
import sys
import time
from typing import Optional


def _run_cmd(cmd: list[str]) -> str:
    try:
        return subprocess.check_output(cmd,
                                       text=True,
                                       stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        return e.output



def get_repo_root() -> str:
    return os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def get_process_cwd(pid: int) -> Optional[str]:
    try:
        res = subprocess.run(
            ['lsof', '-p', str(pid), '-a', '-d', 'cwd', '-F', 'n'],
            capture_output=True,
            text=True,
            check=True)
        for line in res.stdout.splitlines():
            if line.startswith('n'):
                return line[1:]
    except Exception:
        pass
    return None


def get_descendant_pids(pid: int) -> list[int]:
    descendants = []
    try:
        out = subprocess.check_output(['pgrep', '-P', str(pid)], text=True)
        for line in out.splitlines():
            child_pid_str = line.strip()
            if child_pid_str:
                child_pid = int(child_pid_str)
                descendants.append(child_pid)
                descendants.extend(get_descendant_pids(child_pid))
    except subprocess.CalledProcessError:
        pass
    return descendants


def cleanup(session: str, app: Optional[str]) -> int:
    print(f"Cleaning up session '{session}'...")

    # 1. Find and kill all pane processes running in this tmux session
    pane_pids = []
    try:
        out = subprocess.check_output(
            ['tmux', 'list-panes', '-t', session, '-F', '#{pane_pid}'],
            text=True,
            stderr=subprocess.DEVNULL)
        pane_pids = [int(p.strip()) for p in out.splitlines() if p.strip()]
    except Exception:
        pass

    pids_to_kill = []
    for ppid in pane_pids:
        pids_to_kill.extend(get_descendant_pids(ppid))

    for pid in set(pids_to_kill):
        try:
            subprocess.run(['kill', '-9', str(pid)], capture_output=True)
        except Exception:
            pass

    # 2. Kill the tmux session itself
    subprocess.run(['tmux', 'kill-session', '-t', session],
                   capture_output=True)

    # 3. Clean up other workspace-scoped processes
    if app:
        repo_root = get_repo_root()
        res = subprocess.run(['ps', '-x', '-o', 'pid,command'],
                             capture_output=True,
                             text=True)
        if res.returncode == 0:
            for line in res.stdout.splitlines()[1:]:
                line = line.strip()
                if not line:
                    continue
                parts = line.split(None, 1)
                if len(parts) < 2:
                    continue
                pid_str, cmd = parts
                try:
                    pid = int(pid_str)
                except ValueError:
                    continue

                is_target_proc = False
                pattern = (
                    rf'run_app\.py.*{app}|'
                    rf'run_unittests\.py.*{app}|'
                    rf'run_egtests\.py.*{app}'
                )
                if re.search(pattern, cmd):
                    is_target_proc = True

                if is_target_proc:
                    belongs_to_workspace = False
                    if repo_root in cmd:
                        belongs_to_workspace = True
                    else:
                        cwd = get_process_cwd(pid)
                        if cwd and (cwd == repo_root or
                                    cwd.startswith(repo_root + '/')):
                            belongs_to_workspace = True

                    if belongs_to_workspace:
                        try:
                            subprocess.run(['kill', '-9', str(pid)],
                                           capture_output=True)
                        except Exception:
                            pass

    return 0


def start(session: str,
          target: Optional[str],
          app: Optional[str],
          breakpoints: Optional[str],
          pid: Optional[int] = None) -> int:
    cleanup(session, app)
    if pid:
        print(
            f"Starting tmux session '{session}' "
            f"attaching to pid {pid}..."
        )
        cmd = [
            'tmux', 'new-session', '-d', '-s', session,
            f'lldb -p {pid} -O "settings set auto-confirm true"'
        ]
    elif target:
        print(
            f"Starting tmux session '{session}' "
            f"waiting for '{target}'..."
        )
        cmd = [
            'tmux', 'new-session', '-d', '-s', session,
            f'lldb -n {target} --wait-for -O "settings set auto-confirm true"'
        ]
    else:
        print("Error: Either --target or --pid must be specified.")
        return 1

    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Error starting tmux: {res.stderr}")
        return res.returncode
    time.sleep(1)

    if breakpoints:
        for bp in breakpoints.split(','):
            bp_clean = bp.strip()
            if bp_clean:
                send(session, f"breakpoint set -n {bp_clean}", clear=False)
                time.sleep(0.5)
    return 0


def send(session: str, cmd: str, clear: bool) -> int:
    print(f"Sending command to '{session}': {cmd}")
    if clear:
        subprocess.run(['tmux', 'send-keys', '-t', session, 'C-l'])
        subprocess.run(['tmux', 'clear-history', '-t', session])

    subprocess.run(['tmux', 'send-keys', '-t', session, cmd, 'Enter'])
    return 0


def evaluate(session: str, expr: str, timeout: int) -> int:
    print(f"Evaluating expression in '{session}': {expr}")
    subprocess.run(['tmux', 'send-keys', '-t', session, 'C-l'])
    subprocess.run(['tmux', 'clear-history', '-t', session])

    subprocess.run(['tmux', 'send-keys', '-t', session, expr, 'Enter'])

    start_time = time.time()
    while True:
        out = _run_cmd(
            ['tmux', 'capture-pane', '-p', '-t', session, '-S', '-100'])
        lines = [l.strip() for l in out.splitlines() if l.strip()]

        if len(lines) >= 2:
            end_idx = -1
            for i in range(1, len(lines)):
                if lines[i].startswith("(lldb)"):
                    end_idx = i
                    break
            if end_idx != -1:
                result_lines = lines[1:end_idx]
                print("\n".join(result_lines))
                return 0

        if time.time() - start_time > timeout:
            print(f"Timeout ({timeout}s) waiting for evaluation.\n"
                  f"Last output:\n{out}")
            return 1

        time.sleep(1)


def capture(session: str, lines: int, wait_for_stop: bool,
            timeout: int) -> int:
    start_time = time.time()
    while True:
        out = _run_cmd(
            ['tmux', 'capture-pane', '-p', '-t', session, '-S', f'-{lines}'])
        if not wait_for_stop:
            print(out)
            return 0

        if re.search(r'stop reason =|exited with status|Process \d+ exited',
                     out):
            print(out)
            return 0

        if time.time() - start_time > timeout:
            print(f"Timeout ({timeout}s) waiting for stop reason.\n"
                  f"Last output:\n{out}")
            return 1

        time.sleep(2)


def main() -> int:
    epilog_text = """
================================================================================
DETAILED SUBCOMMAND USAGE:
================================================================================

1. start
   Starts a new detached tmux session with LLDB waiting for the target
   executable. Performs multi-session safe cleanup first.

   Arguments:
     --session      Name of the tmux session (required).
     --target       Executable binary name for LLDB to wait for (required).
     --app          App bundle name used for scoping cleanup (optional).
     --breakpoints  Comma-separated list of initial symbolic breakpoints to set
                    (e.g. 'UIViewAlertForUnsatisfiableConstraints,...').

2. send
   Sends an arbitrary LLDB command string to the session.

   Arguments:
     --session      Name of the tmux session (required).
     --cmd          Command string to send (required).
     --clear        Clear screen and history before sending (optional).

3. eval
   Evaluates an LLDB expression command and returns pristine output without
   prompt boilerplate.

   Arguments:
     --session      Name of the tmux session (required).
     --expr         Expression command to evaluate (required, e.g. 'po self').
     --timeout      Timeout in seconds to wait for evaluation (default: 15).

4. capture
   Captures the scrollback buffer of the tmux session.

   Arguments:
     --session      Name of the tmux session (required).
     --lines        Number of scrollback lines to capture (default: 100).
     --wait-for-stop Wait until the process stops or exits (optional).
     --timeout      Timeout in seconds when waiting for stop (default: 30).

5. cleanup
   Cleans up lingering debugger and simulator processes scoped to the target
   app.

   Arguments:
     --session      Name of the tmux session (required).
     --app          App bundle name used for scoping cleanup (optional).
"""
    parser = argparse.ArgumentParser(
        description="Helper for LLDB tmux debugging.",
        epilog=epilog_text,
        formatter_class=argparse.RawTextHelpFormatter)
    subparsers = parser.add_subparsers(dest='command', required=True)

    # start
    parser_start = subparsers.add_parser('start',
                                         help="Start a new lldb tmux session.")
    parser_start.add_argument('--session',
                              required=True,
                              help="Name of tmux session.")
    group = parser_start.add_mutually_exclusive_group(required=True)
    group.add_argument('--target',
                       help="Executable name for lldb to wait for.")
    group.add_argument(
        '--pid',
        type=int,
        help="PID of already running process to attach to.")
    parser_start.add_argument('--app', help="App bundle name for cleanup.")
    parser_start.add_argument(
        '--breakpoints',
        help="Comma-separated list of initial symbolic breakpoints (e.g. "
        "'UIViewAlertForUnsatisfiableConstraints,objc_exception_throw').")

    # send
    parser_send = subparsers.add_parser(
        'send', help="Send an lldb command to the session.")
    parser_send.add_argument('--session',
                             required=True,
                             help="Name of tmux session.")
    parser_send.add_argument('--cmd',
                             required=True,
                             help="Command string to send.")
    parser_send.add_argument('--clear',
                             action='store_true',
                             help="Clear screen and history before sending.")

    # eval
    parser_eval = subparsers.add_parser(
        'eval',
        help="Evaluate an expression command (e.g. 'po self') and return "
        "clean output.",
        description="Sends an LLDB expression evaluation command to the tmux "
        "session, waits for evaluation to complete, and returns pristine "
        "output stripped of LLDB prompt boilerplate.")
    parser_eval.add_argument('--session',
                             required=True,
                             help="Name of tmux session.")
    parser_eval.add_argument(
        '--expr',
        required=True,
        help="Expression command to evaluate (e.g. 'po self').")
    parser_eval.add_argument('--timeout',
                             type=int,
                             default=15,
                             help="Timeout in seconds (default: 15).")

    # capture
    parser_capture = subparsers.add_parser(
        'capture', help="Capture pane output from session.")
    parser_capture.add_argument('--session',
                                required=True,
                                help="Name of tmux session.")
    parser_capture.add_argument(
        '--lines',
        type=int,
        default=100,
        help="Number of scrollback lines (default: 100).")
    parser_capture.add_argument('--wait-for-stop',
                                action='store_true',
                                help="Wait until process stops or exits.")
    parser_capture.add_argument('--timeout',
                                type=int,
                                default=30,
                                help="Timeout in seconds (default: 30).")

    # cleanup
    parser_cleanup = subparsers.add_parser('cleanup',
                                           help="Cleanup session and apps.")
    parser_cleanup.add_argument('--session',
                                required=True,
                                help="Name of tmux session.")
    parser_cleanup.add_argument('--app', help="App bundle name for cleanup.")

    args = parser.parse_args()

    if args.command == 'start':
        return start(args.session, args.target, args.app, args.breakpoints,
                     args.pid)
    elif args.command == 'send':
        return send(args.session, args.cmd, args.clear)
    elif args.command == 'eval':
        return evaluate(args.session, args.expr, args.timeout)
    elif args.command == 'capture':
        return capture(args.session, args.lines, args.wait_for_stop,
                       args.timeout)
    elif args.command == 'cleanup':
        return cleanup(args.session, args.app)

    return 1


if __name__ == '__main__':
    sys.exit(main())
