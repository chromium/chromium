#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Helper script to run the dev CRD multi-process host. Typical usage is:

  $ remoting/tools/run_multi_process_host.py out/debug

The script will elevate itself and ask for your password.
"""

import argparse
import hashlib
import os
import shutil
import socket
import subprocess
import sys


def run_command(command):
    try:
        subprocess.run(command, check=True)
    except subprocess.CalledProcessError as e:
        print(
            "Command failed with exit code "
            f"{e.returncode}: {' '.join(command)}",
            file=sys.stderr)
        print(e.stdout, file=sys.stderr)
        print(e.stderr, file=sys.stderr)
        raise


def systemctl_user(action, unit):
    run_command(["systemctl", "--user", action, unit])


def unset_bad_systemd_env_vars():
    env_vars_to_unset = [
        "PIPEWIRE_REMOTE",
        "PULSE_RUNTIME_PATH",
        "PULSE_SINK",
        "GDK_BACKEND",
        "SSH_CONNECTION",
    ]
    if env_vars_to_unset:
        print("Unsetting systemd user environment variables: "
              f"{' '.join(env_vars_to_unset)}")
        run_command(["systemctl", "--user", "unset-environment"] +
                    env_vars_to_unset)


def user_main(out_dir):
    try:
        run_command(["systemctl", "is-active", "--quiet", "gdm3"])
    except subprocess.CalledProcessError:
        print("gdm3 service is not running.")
        print("Please make sure gdm3.service is unmasked and started:")
        print("    sudo systemctl unmask gdm3.service")
        print("    sudo systemctl start gdm3.service")
        sys.exit(1)

    # The single process host overrides the PipeWire remote, which will break
    # the multi-process host. So we unset these environment variables and
    # restart PipeWire.
    print("Restarting PipeWire...")

    systemctl_user("stop", "pipewire.socket")
    systemctl_user("stop", "pipewire-pulse.socket")

    unset_bad_systemd_env_vars()

    systemctl_user("start", "pipewire.service")
    systemctl_user("start", "pipewire-pulse.service")
    systemctl_user("start", "wireplumber.service")

    print("Re-running script with sudo...")
    script_path = os.path.abspath(__file__)
    home_dir = os.path.expanduser("~")

    run_command([
        "sudo", sys.executable, script_path, "--elevated", out_dir,
        "--user-home", home_dir
    ])


def root_main(out_dir, user_home):
    host_config_path = "/etc/chrome-remote-desktop/host.json"

    if not os.path.exists(host_config_path):
        print(f"{host_config_path} not found. "
              "Looking for the current user config...")
        host_hash = hashlib.md5(socket.gethostname().encode()).hexdigest()
        config_file = os.path.join(
            user_home, f".config/chrome-remote-desktop/host#{host_hash}.json")

        if os.path.exists(config_file):
            os.makedirs(os.path.dirname(host_config_path), exist_ok=True)
            shutil.copy2(config_file, host_config_path)
            print(f"Copied {config_file} to {host_config_path}")
        else:
            print("No suitable user config found")
            sys.exit(1)

    print("Adding _crd_network system user...")
    run_command(["adduser", "--system", "_crd_network"])

    remoting_host_path = os.path.join(out_dir, "remoting_me2me_host")

    # Binaries and various `.so`s need to be readable and executable by
    # `_crd_network` and the `Debian-gdm` group.
    print("Checking permissions...")
    try:
        run_command(
            ["sudo", "-u", "_crd_network", "test", "-x", remoting_host_path])
        run_command([
            "sudo", "-u", "_crd_network", "test", "-r",
            os.path.join(out_dir, "libremoting_core.so")
        ])
        print("_crd_network has the right permissions.")
    except subprocess.CalledProcessError:
        print(
            "_crd_network does not have execute permissions. Setting ACLs...")
        run_command([
            "setfacl", "-R", "-m", "u:_crd_network:rx", "-m",
            "g:Debian-gdm:rx", out_dir
        ])
        print("ACLs updated.")

    # Add an autostart entry for the login session reporter.
    # TODO: crbug.com/488713023 - remove this once we have added support for
    # GNOME 49's dynamic greeter user.
    abs_out_dir = os.path.abspath(out_dir)
    login_reporter_desktop_path = \
        "/usr/share/gdm/greeter/autostart/crd-login-session-reporter.desktop"
    if not os.path.exists(login_reporter_desktop_path):
        print(f"Creating {login_reporter_desktop_path}...")
        desktop_content = f"""[Desktop Entry]
Type=Application
Name=CRD Login Session Reporter
Exec={abs_out_dir}/login_session_reporter
NoDisplay=true
"""
        os.makedirs(os.path.dirname(login_reporter_desktop_path),
                    exist_ok=True)
        with open(login_reporter_desktop_path, "w", encoding='utf-8') as f:
            f.write(desktop_content)
        print(f"{login_reporter_desktop_path} created.")

    daemon_command = \
        [os.path.join(abs_out_dir, "remoting_me2me_host"), "--type=daemon"]
    print(f"Starting CRD host daemon: {' '.join(daemon_command)}")
    try:
        # Use subprocess.run to block and stream output
        subprocess.run(daemon_command, check=True)
    except subprocess.CalledProcessError as e:
        print(
            "Host daemon failed with exit code "
            f"{e.returncode}: {' '.join(daemon_command)}",
            file=sys.stderr)
    except KeyboardInterrupt:
        print("\nCaught KeyboardInterrupt. Stopping host daemon...")
        # subprocess.run handles cleanup on KeyboardInterrupt
    print("CRD host daemon stopped.")

    print("Cleaning up remote sessions...")
    try:
        result = subprocess.run(["loginctl", "list-sessions", "--no-legend"],
                                capture_output=True,
                                text=True,
                                check=True)
        for line in result.stdout.splitlines():
            parts = line.split()
            if not parts:
                continue
            session_id = parts[0]
            show_session_command = [
                "loginctl", "show-session", session_id, "-p", "Remote", "-p",
                "Type"
            ]
            show_result = subprocess.run(show_session_command,
                                         capture_output=True,
                                         text=True,
                                         check=False)
            if show_result.returncode != 0:
                continue

            props = {}
            for show_line in show_result.stdout.splitlines():
                if "=" in show_line:
                    k, v = show_line.split("=", 1)
                    props[k] = v

            if (props.get("Remote") == "yes"
                    and props.get("Type") in ["wayland", "x11"]):
                print(f"Terminating remote session {session_id} "
                      f"(Type={props.get('Type')})...")
                subprocess.run(["loginctl", "terminate-session", session_id],
                               check=False)
    except subprocess.CalledProcessError as e:
        print(f"Failed to list sessions: {e}", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(
        description="Run dev CRD multi-process host.")

    parser.add_argument("out_dir",
                        help="Build output directory (e.g., out/debug)")

    # These should be set by the script itself.
    parser.add_argument("--elevated",
                        action="store_true",
                        help=argparse.SUPPRESS)
    parser.add_argument("--user-home", help=argparse.SUPPRESS)

    args = parser.parse_args()

    if not os.path.isdir(args.out_dir):
        print(f"Error: Output directory not found: {args.out_dir}")
        sys.exit(1)

    if args.elevated:
        if os.geteuid() != 0:
            print("Error: Must be run as root in elevated mode.")
            sys.exit(1)
        if not args.user_home:
            print("Error: --user-home is required in elevated mode.")
            sys.exit(1)
        root_main(args.out_dir, args.user_home)
    else:
        if os.geteuid() == 0:
            print("Error: Do not run this script directly with sudo. "
                  "It will re-invoke itself.")
            sys.exit(1)
        user_main(args.out_dir)


if __name__ == "__main__":
    main()
