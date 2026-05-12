#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Host-driven test to reproduce the MmapPlayback wake lock leak on Android
from crbug.com/495942026.

There are two main cases:
* Renderer process is frozen, leaving the browser's wake lock open
* Browser process is frozen

The browser case seems to be fixed in more recent versions of Android,
but the renderer case is still reproducible. Both cases are retained for
investigation.
"""

import argparse
import json
import logging
import http.server
import os
import re
import socketserver
import subprocess
import sys
import threading
import time

# Add src/build/android to sys.path to import devil.
TOP_SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
sys.path.append(os.path.join(TOP_SRC_DIR, "build", "android"))
sys.path.append(os.path.join(TOP_SRC_DIR, "build", "util"))

try:
    from lib.results import result_sink
    from lib.results import result_types
except ImportError:
    result_sink = None
    result_types = None

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
    test_data_dir = os.path.join(
        TOP_SRC_DIR, "content", "test", "data", "media"
    )
    os.chdir(test_data_dir)
    Handler = http.server.SimpleHTTPRequestHandler
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", SERVER_PORT), Handler) as httpd:
        logging.info(
            "Host: Serving files from %s on port %d", test_data_dir, SERVER_PORT
        )
        httpd.serve_forever()


def get_browser_info(device):
    """Returns the PID and UID of the main browser process."""
    pids = device.GetApplicationPids(TARGET_PACKAGE)
    if not pids:
        logging.error("Could not find browser process.")
        return -1, -1
    assert len(pids) == 1, "Found multiple browser processes."
    pid = pids[0]

    # Alternative to device_utils.GetUidForPackage to avoid dumpsys flakiness
    uid = -1
    pm_output = device.RunShellCommand(
        ["pm", "list", "packages", "-U", TARGET_PACKAGE],
        check_return=True,
        retries=0,
    )
    for line in pm_output:
        if TARGET_PACKAGE in line:
            match = re.search(r"uid:(\d+)", line)
            if match:
                uid = int(match.group(1))
                break
    return pid, uid


def get_renderer_pid(device):
    """Returns the PID of the sandboxed renderer process."""
    procs = device.ListProcesses(
        process_name="org.chromium.content_shell_apk:sandboxed_process"
    )
    if procs:
        # Return the first matching one
        return procs[0].pid, procs[0].name
    return -1, ""


def get_wake_lock_uid(device):
    """Returns the UID attributed to the MmapPlayback/AudioMix wake lock."""
    dumpsys_power = device.RunShellCommand(
        ["dumpsys", "power"], check_return=True, retries=0
    )
    for line in dumpsys_power:
        if "MmapPlayback" in line or "AudioMix" in line:
            logging.debug("Found MmapPlayback/AudioMix line: %s", line.strip())
            # Ignore history lines (they usually start with a date like 04-01)
            if re.match(r"^\d{2}-\d{2}\s+", line.strip()):
                continue

            # Look for WorkSource chains, e.g., WorkChain{(10370), (1041)}
            match = re.search(r"WorkChain\{\((\d+)\)", line)
            if match:
                return int(match.group(1))

            # Look for WorkSource, e.g., ws=WorkSource{10316}
            match = re.search(r"WorkSource\{(\d+)\}", line)
            if match:
                return int(match.group(1))

            # Fallback to uid=NNNN
            match = re.search(r"uid=(\d+)", line)
            if match:
                return int(match.group(1))

    logging.error("No MmapPlayback/AudioMix found in dumpsys power.")
    logging.debug("Active wake locks: %s", dumpsys_power)
    return -1


def launch_content_shell(device, url):
    logging.info(f"Launching Content Shell with URL: {url}")
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
        url,
    ]
    device.RunShellCommand(cmd, check_return=True, retries=0)


def verify_initial_wake_lock(device, expected_uid):
    """
    Poll until we find the initial MmapPlayback/AudioMix wake lock
    associated with audio playback
    """
    wuid = -1
    for attempt in range(5):
        wuid = get_wake_lock_uid(device)
        logging.debug(f"Wake Lock UID (attempt {attempt+1}): {wuid}")
        if wuid == expected_uid:
            break
        logging.info(
            "Attempt %d/5: Wake lock not attributed to browser yet."
            % (attempt + 1)
        )
        time.sleep(2)

    if wuid == -1:
        logging.error(
            "MmapPlayback/AudioMix wake lock not found before freeze."
        )
        return False

    if wuid != expected_uid:
        logging.error(
            "Wake lock UID (%s) does not match Expected UID (%s)",
            wuid,
            expected_uid,
        )
        logging.error(
            "Wake lock does not belong to the browser process (UID mismatch)!"
        )
        return False

    logging.info(
        "Success: MmapPlayback/AudioMix wake lock detected and attributed to browser process."
    )
    return True


def background_app(device):
    logging.info("Backgrounding the application...")
    device.RunShellCommand(
        ["input", "keyevent", "KEYCODE_HOME"],
        check_return=True,
        as_root=True,
        retries=0,
    )
    time.sleep(2)


def get_targets_to_freeze(device, target_type):
    targets = []
    if target_type == "package" or target_type == "both":
        targets.append(TARGET_PACKAGE)
    if target_type == "renderer" or target_type == "both":
        rpid, rname = get_renderer_pid(device)
        if rpid == -1:
            logging.error("Renderer process not found!")
            raise Exception("Renderer process not found")
        logging.info(f"Found Renderer PID: {rpid}, Name: {rname}")
        targets.append(str(rpid))
    return targets


def freeze_targets(device, targets):
    for t in targets:
        logging.info(f"Freezing target: {t}")
        device.RunShellCommand(
            ["am", "freeze", t], check_return=True, as_root=True, retries=0
        )
        # Check if process is still alive after freeze
        if "sandboxed_process" in t:
            procs = device.ListProcesses(process_name=t)
            alive = len(procs) > 0
            logging.info(f"Renderer {t} alive after freeze: {alive}")

    time.sleep(2)


def verify_final_result(device, expect_leak):
    wuid_after = get_wake_lock_uid(device)
    has_lock = wuid_after != -1
    logging.info(f"Wake lock still held after freeze: {has_lock}")

    if expect_leak:
        if has_lock:
            logging.info("Result: PASS (Leak expected and detected)")
            return True
        else:
            logging.info("Result: FAIL (Leak expected but lock was dropped)")
            return False
    else:
        if not has_lock:
            logging.info("Result: PASS (No leak expected and lock was dropped)")
            return True
        else:
            logging.info(
                "Result: FAIL (No leak expected but lock was retained)"
            )
            return False


def cleanup_case(device, changer):
    logging.info("Cleaning up for this case...")
    logging.info(f"Force stopping package: {TARGET_PACKAGE}")
    try:
        device.ForceStop(TARGET_PACKAGE)
    except Exception as e:
        logging.warning(f"Failed to force stop package: {e}")
    logging.info("Restoring flags...")
    changer.Restore()


def run_test_case(device, target, suppress_cleanup):
    """Runs a single test case from the matrix."""
    logging.info(
        "=== Running Test Case: Target=%s, Suppress=%s ==="
        % (target, suppress_cleanup)
    )

    changer = flag_changer.FlagChanger(device, "content-shell-command-line")
    flags = [
        "--disable-gesture-requirement-for-media-playback",
        "--autoplay-policy=no-user-gesture-required",
    ]
    if suppress_cleanup:
        flags.append(f"--{SUPPRESS_CLEANUP_FLAG}")

    logging.info(f"Setting flags: {flags}")
    changer.ReplaceFlags(flags)

    test_url = f"http://localhost:{SERVER_PORT}/{TEST_FILE}"

    try:
        launch_content_shell(device, test_url)
        # Wait a short period for the browser to start
        time.sleep(5)

        bpid, buid = get_browser_info(device)
        logging.info(f"Browser PID: {bpid}, Expected UID: {buid}")

        if not verify_initial_wake_lock(device, buid):
            return False

        background_app(device)

        targets = get_targets_to_freeze(device, target)

        freeze_targets(device, targets)

        return verify_final_result(device, expect_leak=suppress_cleanup)

    finally:
        cleanup_case(device, changer)


def find_apk(build_dir):
    if build_dir:
        path = os.path.join(build_dir, "apks", "ContentShell.apk")
        if os.path.exists(path):
            return path
        path = os.path.join(build_dir, "ContentShell.apk")
        if os.path.exists(path):
            return path

    # Search in out/
    ret = []
    out_dir = os.path.join(TOP_SRC_DIR, "out")
    if os.path.exists(out_dir):
        for d in os.listdir(out_dir):
            path = os.path.join(out_dir, d, "apks", "ContentShell.apk")
            if os.path.exists(path):
                ret.append(path)
                break
            path = os.path.join(out_dir, d, "ContentShell.apk")
            if os.path.exists(path):
                ret.append(path)
                break
    assert (
        len(ret) == 1
    ), f"Expected exactly one apk, found {ret}. Use --build-dir to disambiguate"
    return ret[0]


def setup_device(args):
    instance = None
    device = None

    if args.avd_config:
        logging.info(f"Starting emulator using config: {args.avd_config}")
        from pylib.local.emulator import avd

        avd_config = avd.AvdConfig(args.avd_config)
        avd_config.Install()
        instance = avd_config.StartInstance()
        device = instance.device
    else:
        devices = device_utils.DeviceUtils.HealthyDevices(
            device_arg=args.device
        )
        if not devices:
            logging.error("Error: No healthy devices found.")
            sys.exit(1)
        device = devices[0]

    logging.info(f"Using device: {device.serial}")
    return device, instance


def install_apk(device, build_dir):
    apk_path = find_apk(build_dir)
    assert apk_path, "Error: ContentShell.apk not found"
    logging.info(f"Installing APK: {apk_path}")
    device.Install(apk_path)


def run_scenarios(device, scenarios):
    results = []
    for scenario in scenarios:
        start_scenario = time.time()
        success = False
        try:
            success = run_test_case(
                device, scenario["target"], scenario["suppress"]
            )
        except Exception as e:
            logging.error(
                f"Error running test case for target {scenario['target']}: {e}"
            )
            success = False
        elapsed = time.time() - start_scenario
        results.append((scenario, success, elapsed))
        time.sleep(5)
    return results


def report_results(results, args, start_time):
    logging.info("\n=== Final Results ===")
    all_passed = True
    for scenario, success, elapsed in results:
        res_str = "PASS" if success else "FAIL"
        logging.info(
            f"Target={scenario['target']}, "
            + f" Suppress={scenario['suppress']} -> {res_str}"
        )
        if not success:
            all_passed = False

    if args.isolated_script_test_output:
        results_json = {
            "version": 3,
            "interrupted": False,
            "num_failures_by_type": {},
            "path_delimiter": "/",
            "seconds_since_epoch": start_time,
            "tests": {},
        }

        passes = 0
        failures = 0

        for scenario, success, elapsed in results:
            test_name = (
                f"Target={scenario['target']}_Suppress={scenario['suppress']}"
            )
            status = "PASS" if success else "FAIL"
            if success:
                passes += 1
            else:
                failures += 1

            results_json["tests"][test_name] = {
                "expected": "PASS",
                "actual": status,
                "time": elapsed,
            }

        results_json["num_failures_by_type"] = {
            "PASS": passes,
            "FAIL": failures,
        }

        logging.info(f"Writing results to {args.isolated_script_test_output}")
        with open(args.isolated_script_test_output, "w") as f:
            json.dump(results_json, f)

    # Upload to ResultDB if client is available
    if not result_sink:
        return all_passed

    result_sink_client = result_sink.TryInitClient()
    assert result_sink_client, "No result sink client available"

    logging.info("Uploading results to ResultDB...")
    for scenario, success, elapsed in results:
        test_name = (
            f"Target={scenario['target']} Suppress={scenario['suppress']}"
        )
        status = result_types.PASS if success else result_types.FAIL
        result_sink_client.Post(
            test_name,
            status,
            None,
            None,
            None,
            test_id_structured={
                "coarseName": None,
                "fineName": None,
                # Satisfies the "single" scheme validator
                "caseNameComponents": ["*fixture"],
            },
        )

    return all_passed


def main():
    start_time = time.time()
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dir",
        required=False,
        help="Path to build directory (e.g., out/arm64)",
    )
    parser.add_argument("--device", help="Specific device serial to use")
    parser.add_argument("--avd-config", help="Path to AVD config textpb")
    parser.add_argument(
        "--isolated-script-test-output",
        help="Path to write test results JSON object to",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Enable verbose logging (DEBUG level)",
    )
    args, _ = parser.parse_known_args()

    # Configure logging
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(levelname)s: %(message)s",
    )

    # Initialize devil
    devil_chromium.Initialize(output_directory=args.build_dir)

    device, instance = setup_device(args)
    all_passed = False

    try:
        install_apk(device, args.build_dir)

        # Start local HTTP server in a daemon thread
        server_thread = threading.Thread(target=run_server)
        server_thread.daemon = True
        server_thread.start()
        time.sleep(1)  # Give server a moment to start

        logging.info(
            f"Setting up reverse port forwarding for port {SERVER_PORT}"
        )
        device.adb.Reverse(f"tcp:{SERVER_PORT}", f"tcp:{SERVER_PORT}")

        scenarios = [
            {"target": "package", "suppress": True},
            {"target": "renderer", "suppress": True},
            {"target": "both", "suppress": True},
        ]

        results = run_scenarios(device, scenarios)
        all_passed = report_results(results, args, start_time)

    finally:
        logging.info(f"Removing reverse port forwarding for port {SERVER_PORT}")
        try:
            device.adb.ReverseRemove(f"tcp:{SERVER_PORT}")
        except Exception as e:
            logging.warning(
                f"Notice: Failed to remove reverse port forwarding: {e}"
            )

        if instance:
            logging.info("Stopping emulator instance...")
            instance.Stop()

    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
