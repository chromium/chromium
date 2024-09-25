# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of tests for GPU telemetry tests."""

load("@stdlib//internal/graph.star", "graph")
load("//lib/args.star", args_lib = "args")
load("//lib/structs.star", "structs")
load("../common.star", _targets_common = "common")
load("../nodes.star", _targets_nodes = "nodes")
load("./isolated_script_test.star", "create_isolated_script_test_spec_handler", "isolated_script_test_details")

_isolated_script_test_spec_handler = create_isolated_script_test_spec_handler("GPU telemetry test")

# LINT.IfChange

_BINARY_NAME_BY_BROWSER_CONFIG = {
    _targets_common.browser_config.ANDROID_CHROMIUM: "telemetry_gpu_integration_test_android_chrome",
    _targets_common.browser_config.ANDROID_CHROMIUM_MONOCHROME: "telemetry_gpu_integration_test_android_monochrome",
    _targets_common.browser_config.ANDROID_WEBVIEW: "telemetry_gpu_integration_test_android_webview",
}

def _get_gpu_telemetry_test_binary_node(settings):
    if settings.is_android:
        # TODO: crbug.com/40258588 - Handle equivalent of test_suites key
        # android_webview_gpu_telemetry_tests: telemetry_gpu_integration_test_android_webview
        binary_name = _BINARY_NAME_BY_BROWSER_CONFIG[settings.browser_config]
    elif settings.is_fuchsia:
        binary_name = "telemetry_gpu_integration_test_fuchsia"
    else:
        binary_name = "telemetry_gpu_integration_test"
    return graph.node(_targets_nodes.BINARY.key(binary_name))

def _gpu_telemetry_test_spec_init(node, settings):
    binary_node = _get_gpu_telemetry_test_binary_node(settings)
    spec_value = _isolated_script_test_spec_handler.init(node, settings, binary_node = binary_node)
    spec_value["telemetry_test_name"] = node.props.details.telemetry_test_name
    return spec_value

def _gpu_telemetry_test_spec_finalize(builder_name, test_name, settings, spec_value):
    browser_config = settings.browser_config
    # TODO: crbug.com/40258588 - Handle browser for
    # android_webview_gpu_telemetry_tests and cast_streaming_tests keys

    extra_browser_args = []

    # Most platforms require --enable-logging=stderr to get useful browser logs.
    # However, this actively messes with logging on CrOS (because Chrome's
    # stderr goes nowhere on CrOS) AND --log-level=0 is required for some reason
    # in order to see JavaScript console messages. See
    # https://chromium.googlesource.com/chromium/src.git/+/HEAD/docs/chrome_os_logging.md
    # TODO: crbug.com/40258588 - Handle log level for CrOS
    if settings.is_cros:
        extra_browser_args.append("--log-level=0")
    elif not settings.is_fuchsia or browser_config != "fuchsia-chrome":
        # Stderr logging is not needed for Chrome browser on Fuchsia, as
        # ordinary logging via syslog is captured.
        extra_browser_args.append("--enable-logging=stderr")

    # --expose-gc allows the WebGL conformance tests to more reliably
    # reproduce GC-related bugs in the V8 bindings.
    extra_browser_args.append("--js-flags=--expose-gc")

    spec_value["args"] = args_lib.listify(
        spec_value["telemetry_test_name"] or test_name,
        "--show-stdout",
        "--browser={}".format(browser_config),
        # --passthrough displays more of the logging in Telemetry when run via
        # typ, in particular some of the warnings about tests being expected to
        # fail, but passing.
        "--passthrough",
        "-v",
        "--stable-jobs",
        "--extra-browser-args={}".format(" ".join(extra_browser_args)),
        "--enforce-browser-version",
        spec_value["args"],
    )

    spec_value["swarming"] = structs.evolve(spec_value["swarming"], idempotent = False)

    test_type, sort_key, spec_value = _isolated_script_test_spec_handler.finalize(builder_name, test_name, settings, spec_value)

    spec_value.pop("telemetry_test_name")

    return test_type, sort_key, spec_value

# LINT.ThenChange(//testing/buildbot/generate_buildbot_json.py:gpu_telemetry_test)

_gpu_telemetry_test_spec_handler = _targets_common.spec_handler(
    type_name = _isolated_script_test_spec_handler.type_name,
    init = _gpu_telemetry_test_spec_init,
    finalize = _gpu_telemetry_test_spec_finalize,
)

# TODO(gbeaty) The args that are specified for webgl2?_conformance(.*)_tests
# in the basic suites are pretty formulaic, it would probably make sense to lift
# many of those values into this function
def gpu_telemetry_test(
        *,
        name,
        telemetry_test_name = None,
        args = None,
        mixins = None):
    """Define a GPU telemetry test.

    A GPU telemetry test can be included in a basic suite to run the
    test for any builder that includes that basic suite.

    Args:
        name: The name that can be used to refer to the test in other
            starlark declarations. The step name of the test will be
            based on this name (additional components may be added by
            the recipe or when generating a test with a variant).
        telemetry_test_name: The name of the telemetry benchmark to run.
        mixins: Mixins to apply when expanding the test.
    """
    if not (name.endswith("test") or name.endswith("tests")):
        fail("telemetry test names must end with test or tests, got {}".format(name))

    _targets_common.create_legacy_test(
        name = name,
        basic_suite_test_config = _targets_common.basic_suite_test_config(
            telemetry_test_name = telemetry_test_name,
            args = args,
        ),
        mixins = mixins,
    )

    _targets_common.create_test(
        name = name,
        spec_handler = _gpu_telemetry_test_spec_handler,
        details = isolated_script_test_details(
            args = args,
            additional_fields = dict(
                telemetry_test_name = telemetry_test_name,
            ),
        ),
        mixins = mixins,
    )
