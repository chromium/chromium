#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Host-driven test to reproduce the MmapPlayback wake lock leak on Android
from crbug.com/495942026. This seems to involve process freezing.

There are two main cases:
* Renderer process is frozen, leaving the browser's wake lock open
* Browser process is frozen

The browser case seems to be fixed in a more recent version of Android,
but the renderer case is still reproducible. Both cases are retained for
investigation.
"""

import argparse
import http.server
import os
import re
import socketserver
import subprocess
import sys
import threading
import time

# Add src/build/android to sys.path to import devil.
TOP_SRC_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.append(os.path.join(TOP_SRC_DIR, "build", "android"))

import devil_chromium
from devil.android import device_utils
from devil.android import flag_changer

SERVER_PORT = 8000
TEST_FILE = "aaudio_freezer_test.html"
TARGET_PACKAGE = "org.chromium.content_shell_apk"
ACTIVITY_NAME = "org.chromium.content_shell_apk/.ContentShellActivity"
SUPPRESS_CLEANUP_FLAG = "suppress-aaudio-wakelock-cleanup-for-testing"


def run_server():
  """Starts a simple HTTP server in the directory containing the test file."""
  test_data_dir = os.path.join(TOP_SRC_DIR, "content", "test", "data", "media")
  os.chdir(test_data_dir)
  Handler = http.server.SimpleHTTPRequestHandler
  socketserver.TCPServer.allow_reuse_address = True
  with socketserver.TCPServer(("", SERVER_PORT), Handler) as httpd:
    print(f"Host: Serving files from {test_data_dir} on port {SERVER_PORT}")
    httpd.serve_forever()


def get_browser_info(device):
  """Returns the PID and UID of the main browser process."""
  ps_output = device.RunShellCommand(["ps", "-A"], check_return=True, as_root=True)
  for line in ps_output:
    if TARGET_PACKAGE in line and ":" not in line:
      tokens = line.split()
      if len(tokens) >= 2:
        pid = int(tokens[1])
        user = tokens[0]
        # Convert user like 'u0_a370' to UID 10370
        uid = -1
        if user.startswith("u0_a"):
          try:
            uid = 10000 + int(user[4:])
          except ValueError:
            pass
        return pid, uid
  return -1, -1


def get_renderer_pid(device):
  """Returns the PID of the sandboxed renderer process."""
  ps_output = device.RunShellCommand(["ps", "-A"], check_return=True, as_root=True)
  for line in ps_output:
    if "org.chromium.content_shell_apk:sandboxed_process" in line:
      tokens = line.split()
      if len(tokens) >= 2:
        return int(tokens[1])
  return -1


def get_wake_lock_uid(device):
  """Returns the UID attributed to the MmapPlayback wake lock, or -1.

  It prioritizes parsing the WorkSource in active wake locks.
  """
  dumpsys_power = device.RunShellCommand(["dumpsys", "power"], check_return=True)
  for line in dumpsys_power:
    if "MmapPlayback" in line:
      # Ignore history lines (they usually start with a date like 04-01)
      if re.match(r"^\d{2}-\d{2}\s+", line.strip()):
        continue

      print(f"DEBUG: Found active MmapPlayback line: {line.strip()}")

      # Look for WorkSource chains, e.g., WorkChain{(10370), (1041)}
      match = re.search(r"WorkChain\{\((\d+)\)", line)
      if match:
        return int(match.group(1))

      # Fallback to uid=NNNN
      match = re.search(r"uid=(\d+)", line)
      if match:
        return int(match.group(1))

  return -1


def run_test_case(device, target, suppress_cleanup):
  """Runs a single test case from the matrix."""
  print(f"\n=== Running Test Case: Target={target}, Suppress={suppress_cleanup} ===")

  # Use FlagChanger to set command line flags
  changer = flag_changer.FlagChanger(device, "content-shell-command-line")
  flags = [
    "--disable-gesture-requirement-for-media-playback",
    "--autoplay-policy=no-user-gesture-required",
  ]
  if suppress_cleanup:
    flags.append(f"--{SUPPRESS_CLEANUP_FLAG}")

  print(f"Setting flags: {flags}")
  changer.ReplaceFlags(flags)

  test_url = f"http://localhost:{SERVER_PORT}/{TEST_FILE}"

  try:
    # Launch Content Shell
    print(f"Launching Content Shell with URL: {test_url}")
    cmd = [
      "am",
      "start",
      "-n",
      ACTIVITY_NAME,
      "-a",
      "android.intent.action.MAIN",
      "-c",
      "android.intent.category.LAUNCHER",
      "-d",
      test_url,
    ]
    device.RunShellCommand(cmd, check_return=True)

    print("Waiting for playback to start and wake lock to be acquired...")
    time.sleep(5)

    # Verify wake lock
    wuid = get_wake_lock_uid(device)
    bpid, buid = get_browser_info(device)
    print(f"Browser PID: {bpid}, Expected UID: {buid}")
    print(f"Wake Lock UID: {wuid}")

    if wuid == -1:
      print("Error: MmapPlayback wake lock not found before freeze.")
      return False

    if wuid != buid:
      print(
        f"Warning: Wake lock UID ({wuid}) does not match Expected UID ({buid})"
      )
      print(
        "FAILED: Wake lock does not belong to the browser process (UID mismatch)!"
      )
      return False

    print(
      "Success: MmapPlayback wake lock detected and attributed to browser process."
    )

    # Put the app in the background
    print("Backgrounding the application...")
    device.RunShellCommand(["input", "keyevent", "KEYCODE_HOME"], check_return=True, as_root=True)
    time.sleep(2)

    # Determine targets to freeze
    targets_to_freeze = []
    if target == "package" or target == "both":
      targets_to_freeze.append(TARGET_PACKAGE)
    if target == "renderer" or target == "both":
      rpid = get_renderer_pid(device)
      if rpid == -1:
        print("Error: Renderer process not found!")
        return False
      print(f"Found Renderer PID: {rpid}")
      targets_to_freeze.append(str(rpid))

    # Freeze targets
    for t in targets_to_freeze:
      print(f"Freezing target: {t}")
      device.RunShellCommand(["am", "freeze", t], check_return=True, as_root=True)

      # Verify isfrozen
      try:
        isfrozen_output = device.RunShellCommand(
          ["am", "isfrozen", t], check_return=True, as_root=True
        )
        isfrozen_str = "".join(isfrozen_output).strip()
        print(f"Verify isfrozen for {t}: {isfrozen_str}")
        if "true" not in isfrozen_str.lower():
          print(
            f"Warning: am isfrozen returned '{isfrozen_str}', expected 'true' or similar."
          )
      except Exception as e:
        print(f"Warning: Failed to run am isfrozen: {e}")

    time.sleep(2)

    # Check wake lock status again
    wuid_after = get_wake_lock_uid(device)
    has_lock = wuid_after != -1
    print(f"Wake lock still held after freeze: {has_lock}")

    # Verify against expectation
    expect_leak = suppress_cleanup
    if expect_leak:
      if has_lock:
        print("Result: PASS (Leak expected and detected)")
        return True
      else:
        print("Result: FAIL (Leak expected but lock was dropped)")
        return False
    else:
      if not has_lock:
        print("Result: PASS (No leak expected and lock was dropped)")
        return True
      else:
        print("Result: FAIL (No leak expected but lock was retained)")
        return False

  finally:
    # Cleanup for this case
    print("Cleaning up for this case...")
    print("Restoring flags...")
    changer.Restore()

    # Unfreeze everything just in case
    print(f"Unfreezing package: {TARGET_PACKAGE}")
    try:
      device.RunShellCommand(
        ["am", "unfreeze", TARGET_PACKAGE], check_return=True, as_root=True
      )
    except Exception as e:
      print(f"Notice: Failed to unfreeze package: {e}")

    rpid = get_renderer_pid(device)
    if rpid != -1:
      print(f"Unfreezing renderer: {rpid}")
      try:
        device.RunShellCommand(["am", "unfreeze", str(rpid)], check_return=True)
      except Exception as e:
        print(f"Notice: Failed to unfreeze renderer: {e}")

    print(f"Killing package: {TARGET_PACKAGE}")
    try:
      device.KillAll(TARGET_PACKAGE)
    except Exception as e:
      print(f"Notice: Failed to kill process: {e}")


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
    "--build-dir", required=True, help="Path to build directory (e.g., out/arm64)"
  )
  parser.add_argument("--device", help="Specific device serial to use")
  args = parser.parse_args()

  # Initialize devil
  devil_chromium.Initialize(output_directory=args.build_dir)

  # Find device
  devices = device_utils.DeviceUtils.HealthyDevices(device_arg=args.device)
  if not devices:
    print("Error: No healthy devices found.")
    return 1
  device = devices[0]
  print(f"Using device: {device.serial}")

  # Start local HTTP server in a daemon thread
  server_thread = threading.Thread(target=run_server)
  server_thread.daemon = True
  server_thread.start()
  time.sleep(1)  # Give server a moment to start

  # Set up reverse port forwarding
  try:
    print(f"Setting up reverse port forwarding for port {SERVER_PORT}")
    subprocess.run(
      [
        "adb",
        "-s",
        device.serial,
        "reverse",
        f"tcp:{SERVER_PORT}",
        f"tcp:{SERVER_PORT}",
      ],
      check=True,
    )
  except subprocess.CalledProcessError as e:
    print(f"Error setting up reverse port forwarding: {e}")
    return 1

  # Define test matrix
  scenarios = [
    # TODO(crbug.com/495942026) enable the suppressed cases once this is running
    # in the CQ/trybots.
    # {"target": "package", "suppress": True},
    # {'target': 'package', 'suppress': False},
    {"target": "renderer", "suppress": True},
    # {'target': 'renderer', 'suppress': False},
    # {"target": "both", "suppress": True},
    # {'target': 'both', 'suppress': False},
  ]

  results = []

  try:
    for scenario in scenarios:
      success = run_test_case(device, scenario["target"], scenario["suppress"])
      results.append((scenario, success))
      # Wait a bit between cases
      time.sleep(5)

  finally:
    print("\n=== Final Results ===")
    all_passed = True
    for scenario, success in results:
      res_str = "PASS" if success else "FAIL"
      print(
        f"Target={scenario['target']}, Suppress={scenario['suppress']} -> {res_str}"
      )
      if not success:
        all_passed = False

    print(f"Removing reverse port forwarding for port {SERVER_PORT}")
    try:
      subprocess.run(
        [
          "adb",
          "-s",
          device.serial,
          "reverse",
          "--remove",
          f"tcp:{SERVER_PORT}",
        ],
        check=True,
      )
    except Exception as e:
      print(f"Notice: Failed to remove reverse port forwarding: {e}")
      return 255

  return 0 if all_passed else 1


if __name__ == "__main__":
  sys.exit(main())
