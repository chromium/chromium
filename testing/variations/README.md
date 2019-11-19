# Field Trial Testing Configuration

This directory contains the `fieldtrial_testing_config.json` configuration file,
which is used to ensure test coverage of active field trials.

For each study, the first available experiment after platform filtering is used
as the default experiment for Chromium builds. This experiment is also used for
perf bots and browser tests in the waterfall.

> Note: This configuration applies specifically to Chromium developer builds.
> Chrome branded / official builds do not use these definitions.

> Note: This configuration is NOT used for content_browsertests or other test
> targets based on content_shell.

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
array of *study configurations*. The study name in the configuration file should
match the FieldTrial name used in the Chromium client code.

> Note: Many newer studies do not use study names in the client code at all, and
> rely on the [Feature List API][FeatureListAPI] instead. Nonetheless, if a
> study has a server-side configuration, the study `name` specified here should
> match the name specified in the server-side configuration; this is used to
> implement sanity-checks on the server.

### Study Configurations

Each *study configuration* is a dictionary containing `platforms` and
`experiments`.

`platforms` is an array of strings, indicating the targetted platforms. The
strings may be `android`, `android_webview`, `chromeos`, `ios`, `linux`, `mac`,
or `windows`.

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
the experiment group name. This name should match the FieldTrial experiment
group name used in the Chromium client code.

> Note: Many newer studies do not use experiment names in the client code at
> all, and rely on the [Feature List API][FeatureListAPI] instead. Nonetheless,
> if a study has a server-side configuration, the experiment `name` specified
> here should match the name specified in the server-side configuration; this is
> used to implement sanity-checks on the server.

The remaining keys, `params`, `enable_features`, and `disable_features` are
optional.

`params` is a dictionary mapping parameter name to parameter.

`enable_features` and `disable_features` indicate which features should be
enabled and disabled, respectively, through the
[Feature List API][FeatureListAPI].

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

## Presubmit
The presubmit tool will ensure that your changes follow the correct ordering and
format.
