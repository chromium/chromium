# Field Trial Testing Configuration

This directory contains the `fieldtrial_testing_config.json` configuration file,
which is used to ensure test coverage of active field trials.

For each study, the first available experiment after platform filtering is used
as the default experiment for Chromium builds. This experiment is also used for
perf bots and various tests in the waterfall (browser tests, including those in
browser_tests, components_browsertests, content_browsertests,
extensions_browsertests, interactive_ui_tests, and sync_integration_tests, and
[web platform tests](/docs/testing/web_platform_tests.md)). It is not used by
unit test targets.

> Note: This configuration applies specifically to Chromium developer builds.
> Chrome branded / official builds do not use these definitions by default.
> They can, however, be enabled with the `--enable-field-trial-config` switch.
> For Chrome branded Android builds, due to binary size constraints, the
> configuration cannot be applied by this switch.

> Note: Non-developer builds of Chromium (for example, non-Chrome browsers,
> or Chromium builds provided by Linux distros) should disable the testing
> config by either (1) specifying the GN flag `disable_fieldtrial_testing_config=true`,
> (2) specifying the `--disable-field-trial-config` switch or (3) specifying a
> custom variations server URL using the `--variations-server-url` switch.

> Note: An experiment in the testing configuration file that enables/disables a
> feature that is explicitly overridden (e.g. using the `--enable-features` or
> `--disable-features` switches) will be skipped.

## Config File Format

```json
{
    "StudyName": [
        {
            "platforms": [Array of Strings of Valid Platforms for These Experiments],
            "experiments": [
                {
                    "//0": "Comment Line 0. Lines 0-9 are supported.",
                    "name": "ExperimentName",
                    "params": {Dictionary of Params},
                    "enable_features": [Array of Strings of Features],
                    "disable_features": [Array of Strings of Features]
                },
                ...
            ]
        },
        ...
    ],
    ...
}
```

The config file is a dictionary at the top level mapping a study name to an
array of *study configurations*. The study name in the configuration file
**must** match the FieldTrial name used in the Chromium client code.

> Note: Many newer studies do not use study names in the client code at all, and
> rely on the [Feature List API][FeatureListAPI] instead. Nonetheless, if a
> study has a server-side configuration, the study `name` specified here
> must still match the name specified in the server-side configuration; this is
> used to implement consistency checks on the server.

### Study Configurations

Each *study configuration* is a dictionary containing `platforms` and
`experiments`.

`platforms` is an array of strings, indicating the targetted platforms. The
strings may be `android`, `android_weblayer`, `android_webview`, `chromeos`,
`chromeos_lacros`, `ios`, `linux`, `mac`, or `windows`.

`experiments` is an array containing the *experiments*.

The converter uses the `platforms` array to determine which experiment to use
for the study. The first experiment matching the active platform will be used.

> Note: While `experiments` is defined as an array, currently only the first
> entry is used*\**. We would love to be able to test all possible study
> configurations, but don't currently have the buildbot resources to do so.
> Hence, the current best-practice is to identify which experiment group is the
> most likely candidate for ultimate launch, and to test that configuration. If
> there is a server-side configuration for this study, it's typically
> appropriate to copy/paste one of the experiment definitions into this file.
>
> *\**
> <small>
>   Technically, there is one exception: If there's a forcing_flag group
>   specified in the config, that group will be used if there's a corresponding
>   forcing_flag specified on the command line. You, dear reader, should
>   probably not use this fancy mechanism unless you're <em>quite</em> sure you
>   know what you're doing =)
> </small>

### Experiments (Groups)
Each *experiment* is a dictionary that must contain the `name` key, identifying
the experiment group name.

> Note: Studies should typically use the [Feature List API][FeatureListAPI]. For
> such studies, the experiment `name` specified in the testing config is still
> required (for legacy reasons), but it is ignored. However, the lists of
> `enable_features`, `disable_features`, and `params` **must** match the server
> config. This is enforced via server-side Tricorder checks.
>
> For old-school studies that do check the actual experiment group name in the
> client code, the `name` **must** exactly match the client code and the server
> config.

The remaining keys -- `enable_features`, `disable_features`, `min_os_version`,
and `params` -- are optional.

`enable_features` and `disable_features` indicate which features should be
enabled and disabled, respectively, through the
[Feature List API][FeatureListAPI].

`min_os_version` indicates a minimum OS version level (e.g. "10.0.0") to apply
the experiment. This string is decoded as a `base::Version`. The same version is
applied to all platforms. If you need different versions for different
platforms, you will need to use different studies.

`params` is a dictionary mapping parameter name to parameter value.

> Reminder: The variations framework does not actually fetch any field trial
> definitions from the server for Chromium builds, so any feature enabling or
> disabling must be configured here.

[FeatureListAPI]: https://cs.chromium.org/chromium/src/base/feature_list.h

#### Comments

Each experiment may have up to 10 lines of comments. The comment key must be of
the form `//N` where `N` is between 0 and 9.

```json
{
    "AStudyWithExperimentComment": [
        {
            "platforms": ["chromeos", "linux", "mac", "windows"],
            "experiments": [
                {
                    "//0": "This is the first comment line.",
                    "//1": "This is the second comment line.",
                    "name": "DesktopExperiment"
                }
            ]
        }
    ]
}
```

### Specifying Different Experiments for Different Platforms
Simply specify two different study configurations in the study:

```json
{
    "DifferentExperimentsPerPlatform": [
        {
            "platforms": ["chromeos", "linux", "mac", "windows"],
            "experiments": [{ "name": "DesktopExperiment" }]
        },
        {
            "platforms": ["android", "ios"],
            "experiments": [{ "name": "MobileExperiment" }]
        }
    ]
}
```

## Formatting

Run the following command to auto-format the `fieldtrial_testing_config.json`
configuration file:

```shell
python3 testing/variations/PRESUBMIT.py testing/variations/fieldtrial_testing_config.json
```

The presubmit tool will also ensure that your changes follow the correct
ordering and format.
