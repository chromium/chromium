# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of magic substitutions for starlark.

This module provides the magic_args struct, which has constants providing
argument placeholders. These placeholders enable programatically computing
argument values, avoiding the need for many repetitive
per_test_modifications/entries in test_suite_exceptions.pyl. When generating
targets specs in starlarks, these placeholders will have an associated function
executed to generate args to replace it. When generating pyl files, the
placeholders will be replaced with the magic strings that are used by
//testing/buildbot/generate_buildbot_json.py to detect magic args.

Due to location constraints imposed by lucicfg and angle, it's not possible to
have a single source of truth for the logic for computing these args. The
//testing/buildbot directory is exported to a subtree repo that the angle repo
has a DEPS on, so the logic must be present in //testing/buildbot. lucicfg does
not support loading or reading files from outside of the directory containing
the top-level script (//infra/config). So for the time being, the logic must be
duplicated between this file and
//testing/buildbot/buildbot_json_magic_substitutions.py.
"""

load("./common.star", "common")

def _gpu_device(*, vendor, device):
    return struct(
        vendor = vendor,
        device = device,
    )

# LINT.IfChange

_CROS_BOARD_GPUS = {
    "volteer": _gpu_device(vendor = "8086", device = "9a49"),
}

_VENDOR_SUBSTITUTIONS = {
    "apple": "106b",
    "qcom": "4d4f4351",
}

_DEVICE_SUBSTITUTIONS = {
    "m1": "0",
    "m2": "0",
    # Qualcomm Adreno 680/685/690 and 741 on Windows arm64. The approach
    # swarming uses to find GPUs (looking for all Win32_VideoController WMI
    # objects) results in different output than what Chrome sees.
    # 043a = Adreno 680/685/690 GPU (such as Surface Pro X, Dell trybots)
    # 0636 = Adreno 690 GPU (such as Surface Pro 9 5G)
    # 0c36 = Adreno 741 GPU (such as Surface Pro 11th Edition)
    "043a": "41333430",
    "0636": "36333630",
    "0c36": "36334330",
}

_ANDROID_VULKAN_DEVICES = {
    # Pixel 6 phones map to multiple GPU models.
    "oriole": _gpu_device(vendor = "13b5", device = "92020010,92020000"),
    "dm1q": _gpu_device(vendor = "5143", device = "43050a01"),
    "a23": _gpu_device(vendor = "5143", device = "6010001"),
}

def _get_dimensions(spec_value):
    dimensions = spec_value.get("swarming", {}).get("dimensions")
    if dimensions == None:
        fail("dimensions is not set")
    return dimensions

def _is_skylab(settings):
    """Helper function to determine if the test will be running on skylab."""
    return (settings.browser_config == common.browser_config.CROS_CHROME and
            not settings.use_swarming)

def _get_cros_board_name(spec_value):
    """Helper function to determine what ChromeOS board is being used."""
    dimensions = _get_dimensions(spec_value)
    pool = dimensions.get("pool")
    if not pool:
        fail("No pool set for CrOS test, unable to determine whether running on a VM or physical hardware.")

    if "chrome.tests" not in pool and "chromium.tests" not in pool:
        fail("Unknown CrOS pool {}".format(pool))

    return dimensions.get("device_type", "amd64-generic")

def _cros_telemetry_remote(_, settings, spec_value):
    """Substitutes the correct CrOS remote Telemetry arguments.

    VMs use a hard-coded remote address and port, while physical hardware use
    a magic hostname.
    """
    if _is_skylab(settings):
        # The --remote arument will be automatically added with the correct
        # host name for Skylab
        return []
    if _get_cros_board_name(spec_value) == "amd64-generic":
        return [
            "--remote=127.0.0.1",
            # By default, CrOS VMs' ssh servers listen on local port 9222
            "--remote-ssh-port=9222",
        ]
    return [
        # Magic hostname that resolves to a CrOS device in the test lab
        "--remote=variable_chromeos_device_hostname",
    ]

def _cros_gtest_filter_file(_, settings, spec_value):
    """Substitutes the correct CrOS filter file for gtests."""
    if _is_skylab(settings):
        board = spec_value["cros_board"]
    else:
        board = _get_cros_board_name(spec_value)
    test_name = spec_value["name"]

    # Strip off the variant suffix if it's present.
    if "variant_id" in spec_value:
        test_name = test_name.replace(spec_value["variant_id"], "")
        test_name = test_name.strip()
    filter_file = "chromeos.%s.%s.filter" % (board, test_name)
    return [
        "--test-launcher-filter-file=../../testing/buildbot/filters/" +
        filter_file,
    ]

def _get_gpus(spec_value):
    """Generates all GPU dimension strings from a spec value.

    Returns:
        A list of strings where each string is of the form
        <vendor ID>:<device ID>-<driver>.
    """
    dimensions = _get_dimensions(spec_value)
    if "gpu" not in dimensions:
        return []
    return dimensions["gpu"].split("|")

def _get_android_vulkan_device(settings, spec_value):
    if settings.os_type == common.os_type.ANDROID:
        dimensions = _get_dimensions(spec_value)
        if "device_type" in dimensions:
            return _ANDROID_VULKAN_DEVICES.get(dimensions["device_type"])
    return None

def _gpu_expected_vendor_id(_, settings, spec_value):
    """Substitutes the correct expected GPU vendor for certain GPU tests.

    We only ever trigger tests on a single vendor type per builder definition,
    so multiple found vendors is an error.
    """
    if _is_skylab(settings):
        return _gpu_expected_vendor_id_skylab(settings)
    gpus = _get_gpus(spec_value)

    # We don't specify GPU on things like Android and certain CrOS devices, so
    # default to 0.
    if not gpus:
        return ["--expected-vendor-id", "0"]

    vulkan_device = _get_android_vulkan_device(settings, spec_value)
    if vulkan_device:
        return ["--expected-vendor-id", vulkan_device.vendor]

    vendor_ids = set()
    for gpu_and_driver in gpus:
        # In the form vendor:device-driver.
        vendor = gpu_and_driver.split(":")[0]
        vendor = _VENDOR_SUBSTITUTIONS.get(vendor, vendor)
        vendor_ids.add(vendor)

    if len(vendor_ids) != 1:
        fail("got more than 1 vendor ID")

    return ["--expected-vendor-id", vendor_ids.pop()]

def _gpu_expected_vendor_id_skylab(spec_value):
    cros_board = spec_value.get("cros_board")
    if cros_board == None:
        fail("cros_board is not specified")
    gpu_device = _CROS_BOARD_GPUS.get(cros_board)
    vendor_id = gpu_device.vendor if gpu_device else "0"
    return ["--expected-vendor-id", vendor_id]

def _gpu_expected_device_id(_, settings, spec_value):
    """Substitutes the correct expected GPU(s) for certain GPU tests.

    Most configurations only need one expected GPU, but heterogeneous pools
    (e.g. HD 630 and UHD 630 machines) require multiple.
    """
    if _is_skylab(settings):
        return _gpu_expected_device_id_skylab(spec_value)
    gpus = _get_gpus(spec_value)

    # We don't specify GPU on things like Android/CrOS devices, so default to 0.
    if not gpus:
        return ["--expected-device-id", "0"]

    vulkan_device = _get_android_vulkan_device(settings, spec_value)
    if vulkan_device:
        device_ids = vulkan_device.device.split(",")
        commands = []
        for index, device_id in enumerate(device_ids):
            commands.append("--expected-device-id")
            commands.append(device_ids[index])
        return commands

    device_ids = set()
    for gpu_and_driver in gpus:
        # In the form vendor:device-driver.
        device = gpu_and_driver.split("-")[0].split(":")[1]
        device = _DEVICE_SUBSTITUTIONS.get(device, device)
        device_ids.add(device)

    retval = []
    for device_id in sorted(device_ids):
        retval.extend(["--expected-device-id", device_id])
    return retval

def _gpu_expected_device_id_skylab(spec_value):
    cros_board = spec_value.get("cros_board")
    if cros_board == None:
        fail("cros_board is not specified")
    gpu_device = _CROS_BOARD_GPUS.get(cros_board)
    device_id = gpu_device.device if gpu_device else "0"
    return ["--expected-device-id", device_id]

def _gpu_parallel_jobs(builder_name, settings, spec_value):
    """Substitutes the correct number of jobs for GPU tests.

    Linux/Mac/Windows can run tests in parallel since multiple windows can be
    open but other platforms cannot.
    """
    if not settings.os_type:
        fail("os_type must be specified")

    test_name = spec_value.get("name", "")
    suite = spec_value.get("telemetry_test_name")

    # Return --jobs=1 for Windows Intel bots running the WebGPU CTS
    # These bots can't handle parallel tests. See crbug.com/1353938.
    # The load can also negatively impact WebGL tests, so reduce the number of
    # jobs there.
    # TODO(crbug.com/40233910): Try removing the Windows/Intel special casing once
    # we swap which machines we're using.
    is_webgpu_cts = test_name.startswith("webgpu_cts") or suite == "webgpu_cts"
    is_webgl_cts = (any([n in test_name for n in ("webgl_conformance", "webgl1_conformance", "webgl2_conformance")]) or
                    suite in ("webgl1_conformance", "webgl2_conformance"))
    if settings.os_type == common.os_type.WINDOWS and (is_webgl_cts or is_webgpu_cts):
        for gpu in _get_gpus(spec_value):
            if gpu.startswith("8086"):
                # Especially flaky on '8086:9bc5' per crbug.com/1392149
                if is_webgpu_cts or gpu.startswith("8086:9bc5"):
                    return ["--jobs=1"]
                return ["--jobs=2"]

    # Similarly, the NVIDIA Macbooks are quite old and slow, so reduce the number
    # of jobs there as well.
    if settings.os_type == common.os_type.MAC and is_webgl_cts:
        for gpu in _get_gpus(spec_value):
            if gpu.startswith("10de"):
                return ["--jobs=3"]

    # Slow Mac configs have issues with flakiness when running tests in parallel.
    is_pixel_test = test_name == "pixel_skia_gold_test" or suite == "pixel"
    is_webcodecs_test = test_name == "webcodecs_tests" or suite == "webcodecs"
    is_debug = any([s in builder_name.lower() for s in ("debug", "dbg")])
    if settings.os_type == common.os_type.MAC and (is_pixel_test or is_webcodecs_test):
        if is_debug:
            return ["--jobs=1"]
        for gpu in _get_gpus(spec_value):
            if gpu.startswith("10de"):
                return ["--jobs=1"]

    if settings.os_type in (
        common.os_type.LACROS,
        common.os_type.LINUX,
        common.os_type.MAC,
        common.os_type.WINDOWS,
    ):
        return ["--jobs=4"]
    return ["--jobs=1"]

def _gpu_telemetry_no_root_for_unrooted_devices(_, settings, spec_value):
    """Disables Telemetry's root requests for unrootable Android devices."""
    if not settings.os_type:
        fail("os_type must be specified")
    if settings.os_type != common.os_type.ANDROID:
        return []

    unrooted_devices = (
        "a13",
        "a23",
        "dm1q",  # Samsung S23.
        "devonn",  # Motorola Moto G Power 5G.
    )
    dimensions = _get_dimensions(spec_value)
    device_type = dimensions.get("device_type")
    if device_type in unrooted_devices:
        return ["--compatibility-mode=dont-require-rooted-device"]
    return []

def _gpu_webgl_runtime_file(_, settings, spec_value):
    """Gets the correct WebGL runtime file for a tester."""
    if not settings.os_type:
        fail("os_type must be specified")
    suite = spec_value.get("telemetry_test_name")
    if suite not in ("webgl1_conformance", "webgl2_conformance"):
        fail("unexpected suite: {}".format(suite))

    # Default to using Linux's file if we're on a platform that we don't
    # actively maintain runtime files for.
    chosen_os = settings.os_type
    if chosen_os not in (
        common.os_type.ANDROID,
        common.os_type.LINUX,
        common.os_type.MAC,
        common.os_type.WINDOWS,
    ):
        chosen_os = common.os_type.LINUX

    runtime_filepath = (
        "../../content/test/data/gpu/{}_{}_runtimes.json".format(suite, chosen_os)
    )
    return ["--read-abbreviated-json-results-from={}".format(runtime_filepath)]

_MAGIC_SUBSTITUTION_PREFIX = "$$MAGIC_SUBSTITUTION_"

# LINT.ThenChange(//testing/buildbot/buildbot_json_magic_substitutions.py)

def _placeholder(*, pyl_arg_value, function):
    """Define a placeholder argument that computes programattic replacements.

    Placeholders can be used wherever args for a test are allowed. When
    expanding the test, the placeholder argument will be replaced with a
    sequence of arguments depending on the settings used to expand the test and
    other values in the spec.

    Args:
        pyl_arg_value: The value of the arg that performs the substitution in
            //testing/buildbot. When generating pyl files, this will be written
            out to preserve the existing behavior.
        function: The function to generate the actual argument values when
            processed by starlark. The function should take three arguments:
            * builder_name (str): The name of the builder the test is being
                generated for.
            * settings (targets.settings): A struct providing the configuration
                details of the builder (os_type, browser_config, etc.).
            * spec_value (dict): The test spec. All keys that could be present
                for the test's type will be present, but may be set to None. The
                function should not make modifications to the spec_value.
            The function should return a list of arguments to replace the magic
            argument.
    """
    if not pyl_arg_value.startswith(_MAGIC_SUBSTITUTION_PREFIX):
        fail('pyl_arg_value for gpu magic args must start with "{}", got: "{}"'
            .format(_MAGIC_SUBSTITUTION_PREFIX, pyl_arg_value))
    return struct(
        pyl_arg_value = pyl_arg_value,
        function = function,
    )

magic_args = struct(
    CROS_TELEMETRY_REMOTE = _placeholder(
        pyl_arg_value = "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
        function = _cros_telemetry_remote,
    ),
    CROS_GTEST_FILTER_FILE = _placeholder(
        pyl_arg_value = "$$MAGIC_SUBSTITUTION_ChromeOSGtestFilterFile",
        function = _cros_gtest_filter_file,
    ),
    GPU_EXPECTED_VENDOR_ID = _placeholder(
        pyl_arg_value = "$$MAGIC_SUBSTITUTION_GPUExpectedVendorId",
        function = _gpu_expected_vendor_id,
    ),
    GPU_EXPECTED_DEVICE_ID = _placeholder(
        pyl_arg_value = "$$MAGIC_SUBSTITUTION_GPUExpectedDeviceId",
        function = _gpu_expected_device_id,
    ),
    GPU_PARALLEL_JOBS = _placeholder(
        pyl_arg_value = "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
        function = _gpu_parallel_jobs,
    ),
    GPU_TELEMETRY_NO_ROOT_FOR_UNROOTED_DEVICES = _placeholder(
        pyl_arg_value = "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
        function = _gpu_telemetry_no_root_for_unrooted_devices,
    ),
    GPU_WEBGL_RUNTIME_FILE = _placeholder(
        pyl_arg_value = "$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile",
        function = _gpu_webgl_runtime_file,
    ),
)
